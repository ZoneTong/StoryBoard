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
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTranslator>
#include <QWidget>
#include <QXmlStreamReader>

#include <QHash>

namespace {


UiLanguage gOverride = UiLanguage::System;
bool gOverrideActive = false;

const char* const kTranslationContext = "StoryBoard";
// 源语言是英文；中文等其它语种放在各自的 .ts/.qm 里。
const char* const kTranslationResource = ":/i18n/storyboard_zh.qm";
const char* const kTranslationSourceResource = ":/i18n/storyboard_zh.ts";
// Qt 6 内置控件（QFontDialog、QMessageBox 等）的标准标签由 qtbase 翻译提供。
const char* const kQtTranslationResource = ":/i18n/qtbase_zh_CN.qm";

QTranslator gTranslator;
bool gTranslatorLoaded = false;
bool gTranslatorInstalled = false;
QTranslator gQtTranslator;
bool gQtTranslationAvailable = false;
bool gQtTranslatorInstalled = false;
QHash<QString, QString> gReverseTranslations;

void loadReverseTranslations()
{
    if (!gReverseTranslations.isEmpty())
        return;

    QFile file(QString::fromLatin1(kTranslationSourceResource));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QXmlStreamReader xml(&file);
    QString source;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QLatin1String("source")) {
            source = xml.readElementText();
        } else if (xml.name() == QLatin1String("translation")) {
            const QString translated = xml.readElementText();
            if (!source.isEmpty() && !translated.isEmpty())
                gReverseTranslations.insert(translated, source);
            source.clear();
        }
    }
}

void ensureTranslatorLoaded()
{
    if (gTranslatorLoaded)
        return;
    gTranslatorLoaded = true;
    loadReverseTranslations();
    (void)gTranslator.load(QString::fromLatin1(kTranslationResource));
    gQtTranslationAvailable = gQtTranslator.load(
        QString::fromLatin1(kQtTranslationResource));
}

void installChineseTranslator(bool chinese)
{
    ensureTranslatorLoaded();
    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return;
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
    return QCoreApplication::translate(kTranslationContext, source.toUtf8().constData());
}

UiLanguage gExplicitLanguage = UiLanguage::System;
bool gExplicitActive = false;

// 旧版本把语言单独存在 StoryBoard-language.txt（注册表写不进去时的退路）。
// 这些文件只用于一次性迁移，读到旧值就并入 StoryBoard.ini 再删除，避免两处配置打架。
QStringList legacyLanguageFiles()
{
    static const QStringList files = []() {
        QStringList dirs;
        dirs << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        dirs << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (QCoreApplication::instance())
            dirs << QCoreApplication::applicationDirPath();
        QStringList result;
        for (const QString& dir : dirs) {
            if (dir.isEmpty())
                continue;
            const QString file = QDir(dir).filePath(QStringLiteral("StoryBoard-language.txt"));
            if (!result.contains(file))
                result.append(file);
        }
        return result;
    }();
    return files;
}

QString readPlainLanguage(const QString& file)
{
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly))
        return QString();
    const QStringList lines = QString::fromUtf8(input.readAll()).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("ui/language=")))
            return trimmed.mid(12).trimmed();
    }
    return QString();
}

QString readStoredLanguage()
{
    const QString stored = AppSettings::languageSetting();
    if (!stored.isEmpty())
        return stored;

    for (const QString& file : legacyLanguageFiles()) {
        const QString legacy = readPlainLanguage(file);
        if (legacy.isEmpty())
            continue;
        if (AppSettings::setLanguageSetting(legacy)) {
            for (const QString& other : legacyLanguageFiles())
                QFile::remove(other);
        }
        return legacy;
    }
    // 新装的机器第一次运行就把语言落进 ini，配置文件里能看到这项设置。
    AppSettings::setLanguageSetting(QStringLiteral("system"));
    return QStringLiteral("system");
}

QString& storedLanguageCache()
{
    static QString cached = readStoredLanguage();
    return cached;
}

bool persistLanguage(const QString& stored)
{
    if (!AppSettings::setLanguageSetting(stored))
        return false;
    // 语言已经写进 StoryBoard.ini，旧的独立文件不再保留。
    for (const QString& file : legacyLanguageFiles())
        QFile::remove(file);
    return true;
}

UiLanguage languageFromStored(const QString& stored)
{
    if (stored == QLatin1String("zh"))
        return UiLanguage::Chinese;
    if (stored == QLatin1String("en"))
        return UiLanguage::English;
    return UiLanguage::System;
}

} // namespace

UiLanguage uiLanguageSetting()
{
    if (gExplicitActive)
        return gExplicitLanguage;
    return languageFromStored(storedLanguageCache());
}

void setUiLanguageSetting(UiLanguage language)
{
    const QString stored = language == UiLanguage::Chinese ? QStringLiteral("zh")
        : language == UiLanguage::English ? QStringLiteral("en")
        : QStringLiteral("system");

    // 先记住本次运行的选择：即使磁盘/注册表写失败，当前窗口也能立即切换。
    if (language == UiLanguage::System) {
        gExplicitActive = false;
        gExplicitLanguage = UiLanguage::System;
    } else {
        gExplicitActive = true;
        gExplicitLanguage = language;
    }

    if (persistLanguage(stored))
        storedLanguageCache() = stored;

    installChineseTranslator(uiEffectiveLanguage() == UiLanguage::Chinese);
}

void uiOverrideLanguage(UiLanguage language)
{
    gOverrideActive = language != UiLanguage::System;
    gOverride = language;
    installChineseTranslator(uiEffectiveLanguage() == UiLanguage::Chinese);
}

UiLanguage uiEffectiveLanguage()
{
    if (gOverrideActive)
        return gOverride;
    const QString fromEnv = qEnvironmentVariable("STORYBOARD_LANG").trimmed().toLower();
    if (fromEnv == QLatin1String("zh"))
        return UiLanguage::Chinese;
    if (fromEnv == QLatin1String("en"))
        return UiLanguage::English;
    const UiLanguage setting = uiLanguageSetting();
    if (setting != UiLanguage::System)
        return setting;
    return QLocale::system().language() == QLocale::Chinese ? UiLanguage::Chinese
                                                           : UiLanguage::English;
}

bool uiIsEnglish()
{
    return uiEffectiveLanguage() == UiLanguage::English;
}

void uiInitializeLanguage()
{
    // 语言资源在窗口创建前就装好，首次构造出来的文字直接是目标语言。
    installChineseTranslator(uiEffectiveLanguage() == UiLanguage::Chinese);
}

QString uiText(const QString& english)
{
    return translateToCurrent(english);
}

QString uiTranslateEitherWay(const QString& text)
{
    if (text.isEmpty())
        return text;
    if (uiIsEnglish()) {
        // 源语言是英文；只有已经显示成中文的文本需要查反向表。
        const auto it = gReverseTranslations.constFind(text);
        return it == gReverseTranslations.constEnd() ? text : it.value();
    }
    // 中文翻译器已装载：英文源串会变成中文，本来就是中文的文本保持原样。
    return translateToCurrent(text);
}

void uiRetranslateWidgetTree(QWidget* root)
{
    if (!root)
        return;

    for (QAction* action : root->findChildren<QAction*>()) {
        action->setText(uiTranslateEitherWay(action->text()));
        if (!action->toolTip().isEmpty())
            action->setToolTip(uiTranslateEitherWay(action->toolTip()));
        if (!action->statusTip().isEmpty())
            action->setStatusTip(uiTranslateEitherWay(action->statusTip()));
    }

    for (QMenu* menu : root->findChildren<QMenu*>())
        menu->setTitle(uiTranslateEitherWay(menu->title()));

    for (QDockWidget* dock : root->findChildren<QDockWidget*>())
        dock->setWindowTitle(uiTranslateEitherWay(dock->windowTitle()));

    for (QLabel* label : root->findChildren<QLabel*>())
        label->setText(uiTranslateEitherWay(label->text()));

    for (QAbstractButton* button : root->findChildren<QAbstractButton*>()) {
        button->setText(uiTranslateEitherWay(button->text()));
        if (!button->toolTip().isEmpty())
            button->setToolTip(uiTranslateEitherWay(button->toolTip()));
    }

    for (QGroupBox* group : root->findChildren<QGroupBox*>())
        group->setTitle(uiTranslateEitherWay(group->title()));

    for (QLineEdit* edit : root->findChildren<QLineEdit*>()) {
        if (!edit->placeholderText().isEmpty())
            edit->setPlaceholderText(uiTranslateEitherWay(edit->placeholderText()));
    }

    for (QComboBox* combo : root->findChildren<QComboBox*>()) {
        for (int i = 0; i < combo->count(); ++i) {
            const QString itemText = combo->itemText(i);
            const QString translated = uiTranslateEitherWay(itemText);
            if (translated != itemText)
                combo->setItemText(i, translated);
        }
    }

    for (QTabWidget* tabs : root->findChildren<QTabWidget*>()) {
        for (int i = 0; i < tabs->count(); ++i) {
            const QString tabText = tabs->tabText(i);
            const QString translated = uiTranslateEitherWay(tabText);
            if (translated != tabText)
                tabs->setTabText(i, translated);
        }
    }

    if (root->isWindow())
        root->setWindowTitle(uiTranslateEitherWay(root->windowTitle()));
}
