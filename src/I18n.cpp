#include "I18n.h"

#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QSettings>
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

QTranslator gTranslator;
bool gTranslatorLoaded = false;
bool gTranslatorInstalled = false;
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
}

void installChineseTranslator(bool chinese)
{
    ensureTranslatorLoaded();
    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return;
    if (chinese && !gTranslatorInstalled) {
        app->installTranslator(&gTranslator);
        gTranslatorInstalled = true;
    } else if (!chinese && gTranslatorInstalled) {
        app->removeTranslator(&gTranslator);
        gTranslatorInstalled = false;
    }
}

QString translateToCurrent(const QString& source)
{
    return QCoreApplication::translate(kTranslationContext, source.toUtf8().constData());
}

UiLanguage gExplicitLanguage = UiLanguage::System;
bool gExplicitActive = false;

QString languageKey()
{
    return QStringLiteral("ui/language");
}

// 有些环境（受限的注册表/AppData）里 QSettings 会返回 AccessError，写入静默丢失。
// 这里额外准备几个纯文本落点，注册表写不进去时退到文件，保证语言选择能记住。
QStringList fallbackLanguageFiles()
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

bool writePlainLanguage(const QString& file, const QString& stored)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QFile output(file);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    output.write(QStringLiteral("ui/language=%1\n").arg(stored).toUtf8());
    output.close();
    return readPlainLanguage(file) == stored;
}

QString readStoredLanguage()
{
    for (const QString& file : fallbackLanguageFiles()) {
        const QString plain = readPlainLanguage(file);
        if (!plain.isEmpty())
            return plain;
    }
    QSettings settings;
    if (settings.contains(languageKey()))
        return settings.value(languageKey()).toString();
    return QStringLiteral("system");
}

QString& storedLanguageCache()
{
    static QString cached = readStoredLanguage();
    return cached;
}

bool writeLanguageSetting(QSettings& settings, const QString& stored)
{
    settings.setValue(languageKey(), stored);
    settings.sync();
    return settings.status() == QSettings::NoError && settings.value(languageKey()).toString() == stored;
}

bool persistLanguage(const QString& stored)
{
    QSettings native;
    if (writeLanguageSetting(native, stored)) {
        for (const QString& file : fallbackLanguageFiles())
            QFile::remove(file);
        return true;
    }

    for (const QString& file : fallbackLanguageFiles()) {
        if (writePlainLanguage(file, stored))
            return true;
    }
    return false;
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
