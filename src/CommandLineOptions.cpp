#include "CommandLineOptions.h"

#include <QFileInfo>

namespace {

const QString kSelfTest = QStringLiteral("--self-test");
const QString kRenderDemo = QStringLiteral("--render-demo");
const QString kDemoCanvas = QStringLiteral("--demo-canvas");
const QString kUiScreenshot = QStringLiteral("--ui-screenshot");
const QString kSettingsScreenshot = QStringLiteral("--settings-screenshot");
const QString kSettingsApplyProbe = QStringLiteral("--settings-apply-probe");
const QString kSetLanguage = QStringLiteral("--set-language");
const QString kSettingsProbe = QStringLiteral("--settings-probe");
const QString kAboutScreenshot = QStringLiteral("--about-screenshot");
const QString kItemPropertiesScreenshot = QStringLiteral("--item-properties-screenshot");
const QString kShapeMenuScreenshot = QStringLiteral("--shape-menu-screenshot");
const QString kBrushStyleMenuScreenshot = QStringLiteral("--brush-style-menu-screenshot");
const QString kBrushWidthMenuScreenshot = QStringLiteral("--brush-width-menu-screenshot");
const QString kFillStyleMenuScreenshot = QStringLiteral("--fill-style-menu-screenshot");
const QString kStyleMenuScreenshot = QStringLiteral("--style-menu-screenshot");
const QString kExportScreenshot = QStringLiteral("--export-screenshot");

} // namespace

CommandLineOptions CommandLineOptions::parse(const QStringList& arguments)
{
    CommandLineOptions options;

    options.selfTest = arguments.contains(kSelfTest);
    options.renderDemo = arguments.contains(kRenderDemo);
    options.demoCanvas = arguments.contains(kDemoCanvas);
    options.uiScreenshot = arguments.contains(kUiScreenshot);
    options.settingsScreenshot = arguments.contains(kSettingsScreenshot);
    options.settingsApplyProbe = arguments.contains(kSettingsApplyProbe);
    options.setLanguage = arguments.contains(kSetLanguage);
    options.settingsProbe = arguments.contains(kSettingsProbe);
    options.aboutScreenshot = arguments.contains(kAboutScreenshot);
    options.itemPropertiesScreenshot = arguments.contains(kItemPropertiesScreenshot);
    options.shapeMenuScreenshot = arguments.contains(kShapeMenuScreenshot);
    options.brushStyleMenuScreenshot = arguments.contains(kBrushStyleMenuScreenshot);
    options.brushWidthMenuScreenshot = arguments.contains(kBrushWidthMenuScreenshot);
    options.fillStyleMenuScreenshot = arguments.contains(kFillStyleMenuScreenshot);
    options.styleMenuScreenshot = arguments.contains(kStyleMenuScreenshot);
    options.exportScreenshot = arguments.contains(kExportScreenshot);

    options.selfTestPath = takeOptionPath(arguments, kSelfTest);
    options.renderDemoPath = takeOptionPath(arguments, kRenderDemo);
    options.demoCanvasPath = takeOptionPath(arguments, kDemoCanvas);
    options.uiScreenshotPath = takeOptionPath(arguments, kUiScreenshot);
    options.settingsScreenshotPath = takeOptionPath(arguments, kSettingsScreenshot);
    options.setLanguageValue = takeOptionPath(arguments, kSetLanguage);
    options.settingsProbePath = takeOptionPath(arguments, kSettingsProbe);
    options.aboutScreenshotPath = takeOptionPath(arguments, kAboutScreenshot);
    options.itemPropertiesScreenshotPath = takeOptionPath(arguments, kItemPropertiesScreenshot);
    options.shapeMenuScreenshotPath = takeOptionPath(arguments, kShapeMenuScreenshot);
    options.brushStyleMenuScreenshotPath = takeOptionPath(arguments, kBrushStyleMenuScreenshot);
    options.brushWidthMenuScreenshotPath = takeOptionPath(arguments, kBrushWidthMenuScreenshot);
    options.fillStyleMenuScreenshotPath = takeOptionPath(arguments, kFillStyleMenuScreenshot);
    options.styleMenuScreenshotPath = takeOptionPath(arguments, kStyleMenuScreenshot);
    options.exportScreenshotPath = takeOptionPath(arguments, kExportScreenshot);

    // 位置参数里第一个存在的 .sbcanvas 文件，启动后主窗口打开它。
    const QStringList positional = arguments.mid(1);
    for (const QString& argument : positional) {
        if (argument.startsWith(QLatin1Char('-'))) {
            continue;
        }
        const QString suffix = QFileInfo(argument).suffix().toLower();
        if (QFileInfo::exists(argument) && suffix == QStringLiteral("sbcanvas")) {
            options.canvasPath = argument;
            break;
        }
    }

    return options;
}

QString CommandLineOptions::takeOptionPath(const QStringList& arguments, const QString& option)
{
    const int index = arguments.indexOf(option);
    if (index < 0 || index + 1 >= arguments.size()) {
        return QString();
    }
    const QString value = arguments.at(index + 1);
    if (value.startsWith(QLatin1Char('-'))) {
        return QString();
    }
    return value;
}
