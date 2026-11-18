#include "CommandLineOptions.h"

#include "LineArtPresets.h"

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
const QString kOpenShapesProbe = QStringLiteral("--open-shapes-probe");
const QString kAboutScreenshot = QStringLiteral("--about-screenshot");
const QString kItemPropertiesScreenshot = QStringLiteral("--item-properties-screenshot");
const QString kShapeMenuScreenshot = QStringLiteral("--shape-menu-screenshot");
const QString kBrushStyleMenuScreenshot = QStringLiteral("--brush-style-menu-screenshot");
const QString kBrushWidthMenuScreenshot = QStringLiteral("--brush-width-menu-screenshot");
const QString kFillStyleMenuScreenshot = QStringLiteral("--fill-style-menu-screenshot");
const QString kStyleMenuScreenshot = QStringLiteral("--style-menu-screenshot");
const QString kExportScreenshot = QStringLiteral("--export-screenshot");
const QString kLineArt = QStringLiteral("--lineart");
const QString kLineArtOut = QStringLiteral("--lineart-out");
const QString kLineArtPreset = QStringLiteral("--lineart-preset");
const QString kLineArtMaxSide = QStringLiteral("--lineart-max-side");
const QString kLineArtColorLevels = QStringLiteral("--lineart-color-levels");
const QString kLineArtPaletteColors = QStringLiteral("--lineart-palette-colors");
const QString kLineArtPaletteMerge = QStringLiteral("--lineart-palette-merge");
const QString kLineArtAdjacentMerge = QStringLiteral("--lineart-adjacent-merge");
const QString kLineArtMinArea = QStringLiteral("--lineart-min-area");
const QString kLineArtMaxRegions = QStringLiteral("--lineart-max-regions");
const QString kLineArtAdaptive = QStringLiteral("--lineart-adaptive");
const QString kLineArtAdaptiveLimit = QStringLiteral("--lineart-adaptive-limit");
const QString kLineArtMinAdaptiveSide = QStringLiteral("--lineart-min-adaptive-side");
const QString kLineArtTolerance = QStringLiteral("--lineart-tolerance");
const QString kLineArtMinThickness = QStringLiteral("--lineart-min-thickness");

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
    options.openShapesProbe = arguments.contains(kOpenShapesProbe);
    options.aboutScreenshot = arguments.contains(kAboutScreenshot);
    options.itemPropertiesScreenshot = arguments.contains(kItemPropertiesScreenshot);
    options.shapeMenuScreenshot = arguments.contains(kShapeMenuScreenshot);
    options.brushStyleMenuScreenshot = arguments.contains(kBrushStyleMenuScreenshot);
    options.brushWidthMenuScreenshot = arguments.contains(kBrushWidthMenuScreenshot);
    options.fillStyleMenuScreenshot = arguments.contains(kFillStyleMenuScreenshot);
    options.styleMenuScreenshot = arguments.contains(kStyleMenuScreenshot);
    options.exportScreenshot = arguments.contains(kExportScreenshot);
    options.lineArt = arguments.contains(kLineArt);

    options.selfTestPath = takeOptionPath(arguments, kSelfTest);
    options.renderDemoPath = takeOptionPath(arguments, kRenderDemo);
    options.demoCanvasPath = takeOptionPath(arguments, kDemoCanvas);
    options.uiScreenshotPath = takeOptionPath(arguments, kUiScreenshot);
    options.settingsScreenshotPath = takeOptionPath(arguments, kSettingsScreenshot);
    options.setLanguageValue = takeOptionPath(arguments, kSetLanguage);
    options.settingsProbePath = takeOptionPath(arguments, kSettingsProbe);
    options.openShapesProbePath = takeOptionPath(arguments, kOpenShapesProbe);
    options.aboutScreenshotPath = takeOptionPath(arguments, kAboutScreenshot);
    options.itemPropertiesScreenshotPath = takeOptionPath(arguments, kItemPropertiesScreenshot);
    options.shapeMenuScreenshotPath = takeOptionPath(arguments, kShapeMenuScreenshot);
    options.brushStyleMenuScreenshotPath = takeOptionPath(arguments, kBrushStyleMenuScreenshot);
    options.brushWidthMenuScreenshotPath = takeOptionPath(arguments, kBrushWidthMenuScreenshot);
    options.fillStyleMenuScreenshotPath = takeOptionPath(arguments, kFillStyleMenuScreenshot);
    options.styleMenuScreenshotPath = takeOptionPath(arguments, kStyleMenuScreenshot);
    options.exportScreenshotPath = takeOptionPath(arguments, kExportScreenshot);

    // ---- 命令行线稿化参数 ----
    options.lineArtInputPath = takeOptionPath(arguments, kLineArt);
    options.lineArtOutputPath = takeOptionPath(arguments, kLineArtOut);
    const QString presetValue = takeOptionPath(arguments, kLineArtPreset);
    if (!presetValue.isEmpty()) {
        bool presetOk = false;
        const int preset = presetValue.toInt(&presetOk);
        if (presetOk && preset >= 0 && preset < lineArtPresetCount()) {
            options.lineArtPreset = preset;
        }
    }
    auto takeInt = [&arguments](const QString& option, bool& seen, int& value) {
        const QString text = takeOptionPath(arguments, option);
        if (text.isEmpty()) {
            return;
        }
        bool ok = false;
        const int parsed = text.toInt(&ok);
        if (ok) {
            seen = true;
            value = parsed;
        }
    };
    auto takeDouble = [&arguments](const QString& option, bool& seen, double& value) {
        const QString text = takeOptionPath(arguments, option);
        if (text.isEmpty()) {
            return;
        }
        bool ok = false;
        const double parsed = text.toDouble(&ok);
        if (ok) {
            seen = true;
            value = parsed;
        }
    };
    takeInt(kLineArtMaxSide, options.lineArtMaxSideSeen, options.lineArtMaxSide);
    takeInt(kLineArtColorLevels, options.lineArtColorLevelsSeen, options.lineArtColorLevels);
    takeInt(kLineArtPaletteColors, options.lineArtPaletteColorsSeen, options.lineArtPaletteColors);
    takeDouble(kLineArtPaletteMerge, options.lineArtPaletteMergeSeen, options.lineArtPaletteMerge);
    takeDouble(kLineArtAdjacentMerge, options.lineArtAdjacentMergeSeen, options.lineArtAdjacentMerge);
    takeInt(kLineArtMinArea, options.lineArtMinAreaSeen, options.lineArtMinArea);
    takeInt(kLineArtMaxRegions, options.lineArtMaxRegionsSeen, options.lineArtMaxRegions);
    takeInt(kLineArtAdaptiveLimit, options.lineArtAdaptiveLimitSeen, options.lineArtAdaptiveLimit);
    takeInt(kLineArtMinAdaptiveSide, options.lineArtMinAdaptiveSideSeen, options.lineArtMinAdaptiveSide);
    takeDouble(kLineArtTolerance, options.lineArtToleranceSeen, options.lineArtTolerance);
    takeDouble(kLineArtMinThickness, options.lineArtMinThicknessSeen, options.lineArtMinThickness);
    // 自适应开关取 0/1；其它值一律视为未给出，保持默认开启。
    {
        const QString text = takeOptionPath(arguments, kLineArtAdaptive);
        if (text == QStringLiteral("0") || text.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
            options.lineArtAdaptiveSeen = true;
            options.lineArtAdaptive = false;
        } else if (text == QStringLiteral("1") || text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
            options.lineArtAdaptiveSeen = true;
            options.lineArtAdaptive = true;
        }
    }

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
