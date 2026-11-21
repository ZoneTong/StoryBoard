#include "I18n.h"

#include "Settings.h"

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTranslator>
#include <QWidget>
#include <QXmlStreamReader>

namespace {


UiLanguage gOverride = UiLanguage::SYSTEM;
bool gOverrideActive = false;

const char* const TRANSLATION_CONTEXT = "StoryBoard";
// 源语言是英文；中文等其它语种放在各自的 .ts/.qm 里。
const char* const TRANSLATION_RESOURCE = ":/i18n/storyboard_zh.qm";
const char* const TRANSLATION_SOURCE_RESOURCE = ":/i18n/storyboard_zh.ts";
// Qt 6 内置控件（QFontDialog、QMessageBox 等）的标准标签由 qtbase 翻译提供。
const char* const QT_TRANSLATION_RESOURCE = ":/i18n/qtbase_zh_CN.qm";

QTranslator gTranslator;
bool gTranslatorLoaded = false;
bool gTranslatorInstalled = false;
QTranslator gQtTranslator;
bool gQtTranslationAvailable = false;
bool gQtTranslatorInstalled = false;
QHash<QString, QString> gReverseTranslations;

void loadReverseTranslations()
{
    if (!gReverseTranslations.isEmpty()) {
        return;
    }

    QFile file(QString::fromLatin1(TRANSLATION_SOURCE_RESOURCE));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QXmlStreamReader xml(&file);
    QString source;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }
        if (xml.name() == QLatin1String("source")) {
            source = xml.readElementText();
        } else if (xml.name() == QLatin1String("translation")) {
            const QString translated = xml.readElementText();
            if (!source.isEmpty() && !translated.isEmpty()) {
                gReverseTranslations.insert(translated, source);
            }
            source.clear();
        }
    }
}

void ensureTranslatorLoaded()
{
    if (gTranslatorLoaded) {
        return;
    }
    gTranslatorLoaded = true;
    loadReverseTranslations();
    (void)gTranslator.load(QString::fromLatin1(TRANSLATION_RESOURCE));
    gQtTranslationAvailable = gQtTranslator.load(QString::fromLatin1(QT_TRANSLATION_RESOURCE));
}

void installChineseTranslator(bool chinese)
{
    ensureTranslatorLoaded();
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        return;
    }
    if (chinese) {
        if (!gTranslatorInstalled) {
            app->installTranslator(&gTranslator);
            gTranslatorInstalled = true;
        }
        // qtbase 翻译放在最后安装：Qt 内置控件优先查到它，应用自己的
        // StoryBoard 上下文仍由 gTranslator 提供。
        if (gQtTranslationAvailable && !gQtTranslatorInstalled) {
            app->installTranslator(&gQtTranslator);
            gQtTranslatorInstalled = true;
        }
    } else {
        if (gQtTranslatorInstalled) {
            app->removeTranslator(&gQtTranslator);
            gQtTranslatorInstalled = false;
        }
        if (gTranslatorInstalled) {
            app->removeTranslator(&gTranslator);
            gTranslatorInstalled = false;
        }
    }
}

QString translateToCurrent(const QString& source)
{
    return QCoreApplication::translate(TRANSLATION_CONTEXT, source.toUtf8().constData());
}

UiLanguage gExplicitLanguage = UiLanguage::SYSTEM;
bool gExplicitActive = false;

// 旧版本把语言单独存在 StoryBoard-language.txt（注册表写不进去时的退路）。
// 这些文件只用于一次性迁移，读到旧值就并入 StoryBoard.ini 再删除，避免两处配置打架。
QStringList legacyLanguageFiles()
{
    static const QStringList files = []() {
        QStringList dirs;
        dirs << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        dirs << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (QCoreApplication::instance()) {
            dirs << QCoreApplication::applicationDirPath();
        }
        QStringList result;
        for (const QString& dir : dirs) {
            if (dir.isEmpty()) {
                continue;
            }
            const QString file = QDir(dir).filePath(QStringLiteral("StoryBoard-language.txt"));
            if (!result.contains(file)) {
                result.append(file);
            }
        }
        return result;
    }();
    return files;
}

QString readPlainLanguage(const QString& file)
{
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QStringList lines = QString::fromUtf8(input.readAll()).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("ui/language="))) {
            return trimmed.mid(12).trimmed();
        }
    }
    return QString();
}

QString readStoredLanguage()
{
    const QString stored = AppSettings::LanguageSetting();
    if (!stored.isEmpty()) {
        return stored;
    }

    for (const QString& file : legacyLanguageFiles()) {
        const QString legacy = readPlainLanguage(file);
        if (legacy.isEmpty()) {
            continue;
        }
        if (AppSettings::SetLanguageSetting(legacy)) {
            for (const QString& other : legacyLanguageFiles()) {
                QFile::remove(other);
            }
        }
        return legacy;
    }
    // 新装的机器第一次运行就把语言落进 ini，配置文件里能看到这项设置。
    AppSettings::SetLanguageSetting(QStringLiteral("system"));
    return QStringLiteral("system");
}

QString& storedLanguageCache()
{
    static QString cached = readStoredLanguage();
    return cached;
}

bool persistLanguage(const QString& stored)
{
    if (!AppSettings::SetLanguageSetting(stored)) {
        return false;
    }
    // 语言已经写进 StoryBoard.ini，旧的独立文件不再保留。
    for (const QString& file : legacyLanguageFiles()) {
        QFile::remove(file);
    }
    return true;
}

UiLanguage languageFromStored(const QString& stored)
{
    if (stored == QLatin1String("zh")) {
        return UiLanguage::CHINESE;
    }
    if (stored == QLatin1String("en")) {
        return UiLanguage::ENGLISH;
    }
    return UiLanguage::SYSTEM;
}

} // namespace

UiLanguage UiLanguageSetting()
{
    if (gExplicitActive) {
        return gExplicitLanguage;
    }
    return languageFromStored(storedLanguageCache());
}

void SetUiLanguageSetting(UiLanguage language)
{
    const QString stored = language == UiLanguage::CHINESE   ? QStringLiteral("zh")
                           : language == UiLanguage::ENGLISH ? QStringLiteral("en")
                                                             : QStringLiteral("system");

    // 先记住本次运行的选择：即使磁盘/注册表写失败，当前窗口也能立即切换。
    if (language == UiLanguage::SYSTEM) {
        gExplicitActive = false;
        gExplicitLanguage = UiLanguage::SYSTEM;
    } else {
        gExplicitActive = true;
        gExplicitLanguage = language;
    }

    if (persistLanguage(stored)) {
        storedLanguageCache() = stored;
    }

    installChineseTranslator(UiEffectiveLanguage() == UiLanguage::CHINESE);
}

void UiOverrideLanguage(UiLanguage language)
{
    gOverrideActive = language != UiLanguage::SYSTEM;
    gOverride = language;
    installChineseTranslator(UiEffectiveLanguage() == UiLanguage::CHINESE);
}

UiLanguage UiEffectiveLanguage()
{
    if (gOverrideActive) {
        return gOverride;
    }
    const QString fromEnv = qEnvironmentVariable("STORYBOARD_LANG").trimmed().toLower();
    if (fromEnv == QLatin1String("zh")) {
        return UiLanguage::CHINESE;
    }
    if (fromEnv == QLatin1String("en")) {
        return UiLanguage::ENGLISH;
    }
    const UiLanguage setting = UiLanguageSetting();
    if (setting != UiLanguage::SYSTEM) {
        return setting;
    }
    return QLocale::system().language() == QLocale::Chinese ? UiLanguage::CHINESE : UiLanguage::ENGLISH;
}

bool UiIsEnglish()
{
    return UiEffectiveLanguage() == UiLanguage::ENGLISH;
}

void UiInitializeLanguage()
{
    // 语言资源在窗口创建前就装好，首次构造出来的文字直接是目标语言。
    installChineseTranslator(UiEffectiveLanguage() == UiLanguage::CHINESE);
}

QString UiText(const QString& english)
{
    return translateToCurrent(english);
}

QString UiTranslateEitherWay(const QString& text)
{
    if (text.isEmpty()) {
        return text;
    }
    if (UiIsEnglish()) {
        // 源语言是英文；只有已经显示成中文的文本需要查反向表。
        const auto it = gReverseTranslations.constFind(text);
        return it == gReverseTranslations.constEnd() ? text : it.value();
    }
    // 中文翻译器已装载：英文源串会变成中文，本来就是中文的文本保持原样。
    return translateToCurrent(text);
}

void UiRetranslateWidgetTree(QWidget* root)
{
    if (!root) {
        return;
    }

    for (QAction* action : root->findChildren<QAction*>()) {
        action->setText(UiTranslateEitherWay(action->text()));
        if (!action->toolTip().isEmpty()) {
            action->setToolTip(UiTranslateEitherWay(action->toolTip()));
        }
        if (!action->statusTip().isEmpty()) {
            action->setStatusTip(UiTranslateEitherWay(action->statusTip()));
        }
    }

    for (QMenu* menu : root->findChildren<QMenu*>()) {
        menu->setTitle(UiTranslateEitherWay(menu->title()));
    }

    for (QDockWidget* dock : root->findChildren<QDockWidget*>()) {
        dock->setWindowTitle(UiTranslateEitherWay(dock->windowTitle()));
    }

    for (QLabel* label : root->findChildren<QLabel*>()) {
        label->setText(UiTranslateEitherWay(label->text()));
    }

    for (QAbstractButton* button : root->findChildren<QAbstractButton*>()) {
        button->setText(UiTranslateEitherWay(button->text()));
        if (!button->toolTip().isEmpty()) {
            button->setToolTip(UiTranslateEitherWay(button->toolTip()));
        }
    }

    for (QGroupBox* group : root->findChildren<QGroupBox*>()) {
        group->setTitle(UiTranslateEitherWay(group->title()));
    }

    for (QLineEdit* edit : root->findChildren<QLineEdit*>()) {
        if (!edit->placeholderText().isEmpty()) {
            edit->setPlaceholderText(UiTranslateEitherWay(edit->placeholderText()));
        }
    }

    for (QComboBox* combo : root->findChildren<QComboBox*>()) {
        for (int i = 0; i < combo->count(); ++i) {
            const QString itemText = combo->itemText(i);
            const QString translated = UiTranslateEitherWay(itemText);
            if (translated != itemText) {
                combo->setItemText(i, translated);
            }
        }
    }

    for (QTabWidget* tabs : root->findChildren<QTabWidget*>()) {
        for (int i = 0; i < tabs->count(); ++i) {
            const QString tabText = tabs->tabText(i);
            const QString translated = UiTranslateEitherWay(tabText);
            if (translated != tabText) {
                tabs->setTabText(i, translated);
            }
        }
    }

    if (root->isWindow()) {
        root->setWindowTitle(UiTranslateEitherWay(root->windowTitle()));
    }
}
