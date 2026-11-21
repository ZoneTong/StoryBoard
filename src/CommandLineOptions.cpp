#include "CommandLineOptions.h"

#include "LineArtPresets.h"

#include <QFileInfo>

namespace {

const QString SELF_TEST = QStringLiteral("--self-test");
const QString RENDER_DEMO = QStringLiteral("--render-demo");
const QString DEMO_CANVAS = QStringLiteral("--demo-canvas");
const QString UI_SCREENSHOT = QStringLiteral("--ui-screenshot");
const QString SETTINGS_SCREENSHOT = QStringLiteral("--settings-screenshot");
const QString SETTINGS_APPLY_PROBE = QStringLiteral("--settings-apply-probe");
const QString SET_LANGUAGE = QStringLiteral("--set-language");
const QString SETTINGS_PROBE = QStringLiteral("--settings-probe");
const QString OPEN_SHAPES_PROBE = QStringLiteral("--open-shapes-probe");
const QString ABOUT_SCREENSHOT = QStringLiteral("--about-screenshot");
const QString ITEM_PROPERTIES_SCREENSHOT = QStringLiteral("--item-properties-screenshot");
const QString SHAPE_MENU_SCREENSHOT = QStringLiteral("--shape-menu-screenshot");
const QString BRUSH_STYLE_MENU_SCREENSHOT = QStringLiteral("--brush-style-menu-screenshot");
const QString BRUSH_WIDTH_MENU_SCREENSHOT = QStringLiteral("--brush-width-menu-screenshot");
const QString FILL_STYLE_MENU_SCREENSHOT = QStringLiteral("--fill-style-menu-screenshot");
const QString STYLE_MENU_SCREENSHOT = QStringLiteral("--style-menu-screenshot");
const QString EXPORT_SCREENSHOT = QStringLiteral("--export-screenshot");
const QString LINE_ART = QStringLiteral("--lineart");
const QString LINE_ART_OUT = QStringLiteral("--lineart-out");
const QString LINE_ART_PRESET = QStringLiteral("--lineart-preset");
const QString LINE_ART_MAX_SIDE = QStringLiteral("--lineart-max-side");
const QString LINE_ART_COLOR_LEVELS = QStringLiteral("--lineart-color-levels");
const QString LINE_ART_PALETTE_COLORS = QStringLiteral("--lineart-palette-colors");
const QString LINE_ART_PALETTE_MERGE = QStringLiteral("--lineart-palette-merge");
const QString LINE_ART_ADJACENT_MERGE = QStringLiteral("--lineart-adjacent-merge");
const QString LINE_ART_MIN_AREA = QStringLiteral("--lineart-min-area");
const QString LINE_ART_MAX_REGIONS = QStringLiteral("--lineart-max-regions");
const QString LINE_ART_ADAPTIVE = QStringLiteral("--lineart-adaptive");
const QString LINE_ART_ADAPTIVE_LIMIT = QStringLiteral("--lineart-adaptive-limit");
const QString LINE_ART_MIN_ADAPTIVE_SIDE = QStringLiteral("--lineart-min-adaptive-side");
const QString LINE_ART_TOLERANCE = QStringLiteral("--lineart-tolerance");
const QString LINE_ART_MIN_THICKNESS = QStringLiteral("--lineart-min-thickness");

} // namespace

CommandLineOptions CommandLineOptions::parse(const QStringList& arguments)
{
    CommandLineOptions options;

    options.selfTest = arguments.contains(SELF_TEST);
    options.renderDemo = arguments.contains(RENDER_DEMO);
    options.demoCanvas = arguments.contains(DEMO_CANVAS);
    options.uiScreenshot = arguments.contains(UI_SCREENSHOT);
    options.settingsScreenshot = arguments.contains(SETTINGS_SCREENSHOT);
    options.settingsApplyProbe = arguments.contains(SETTINGS_APPLY_PROBE);
    options.setLanguage = arguments.contains(SET_LANGUAGE);
    options.settingsProbe = arguments.contains(SETTINGS_PROBE);
    options.openShapesProbe = arguments.contains(OPEN_SHAPES_PROBE);
    options.aboutScreenshot = arguments.contains(ABOUT_SCREENSHOT);
    options.itemPropertiesScreenshot = arguments.contains(ITEM_PROPERTIES_SCREENSHOT);
    options.shapeMenuScreenshot = arguments.contains(SHAPE_MENU_SCREENSHOT);
    options.brushStyleMenuScreenshot = arguments.contains(BRUSH_STYLE_MENU_SCREENSHOT);
    options.brushWidthMenuScreenshot = arguments.contains(BRUSH_WIDTH_MENU_SCREENSHOT);
    options.fillStyleMenuScreenshot = arguments.contains(FILL_STYLE_MENU_SCREENSHOT);
    options.styleMenuScreenshot = arguments.contains(STYLE_MENU_SCREENSHOT);
    options.exportScreenshot = arguments.contains(EXPORT_SCREENSHOT);
    options.lineArt = arguments.contains(LINE_ART);

    options.selfTestPath = takeOptionPath(arguments, SELF_TEST);
    options.renderDemoPath = takeOptionPath(arguments, RENDER_DEMO);
    options.demoCanvasPath = takeOptionPath(arguments, DEMO_CANVAS);
    options.uiScreenshotPath = takeOptionPath(arguments, UI_SCREENSHOT);
    options.settingsScreenshotPath = takeOptionPath(arguments, SETTINGS_SCREENSHOT);
    options.setLanguageValue = takeOptionPath(arguments, SET_LANGUAGE);
    options.settingsProbePath = takeOptionPath(arguments, SETTINGS_PROBE);
    options.openShapesProbePath = takeOptionPath(arguments, OPEN_SHAPES_PROBE);
    options.aboutScreenshotPath = takeOptionPath(arguments, ABOUT_SCREENSHOT);
    options.itemPropertiesScreenshotPath = takeOptionPath(arguments, ITEM_PROPERTIES_SCREENSHOT);
    options.shapeMenuScreenshotPath = takeOptionPath(arguments, SHAPE_MENU_SCREENSHOT);
    options.brushStyleMenuScreenshotPath = takeOptionPath(arguments, BRUSH_STYLE_MENU_SCREENSHOT);
    options.brushWidthMenuScreenshotPath = takeOptionPath(arguments, BRUSH_WIDTH_MENU_SCREENSHOT);
    options.fillStyleMenuScreenshotPath = takeOptionPath(arguments, FILL_STYLE_MENU_SCREENSHOT);
    options.styleMenuScreenshotPath = takeOptionPath(arguments, STYLE_MENU_SCREENSHOT);
    options.exportScreenshotPath = takeOptionPath(arguments, EXPORT_SCREENSHOT);

    // ---- 命令行线稿化参数 ----
    options.lineArtInputPath = takeOptionPath(arguments, LINE_ART);
    options.lineArtOutputPath = takeOptionPath(arguments, LINE_ART_OUT);
    const QString presetValue = takeOptionPath(arguments, LINE_ART_PRESET);
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
    takeInt(LINE_ART_MAX_SIDE, options.lineArtMaxSideSeen, options.lineArtMaxSide);
    takeInt(LINE_ART_COLOR_LEVELS, options.lineArtColorLevelsSeen, options.lineArtColorLevels);
    takeInt(LINE_ART_PALETTE_COLORS, options.lineArtPaletteColorsSeen, options.lineArtPaletteColors);
    takeDouble(LINE_ART_PALETTE_MERGE, options.lineArtPaletteMergeSeen, options.lineArtPaletteMerge);
    takeDouble(LINE_ART_ADJACENT_MERGE, options.lineArtAdjacentMergeSeen, options.lineArtAdjacentMerge);
    takeInt(LINE_ART_MIN_AREA, options.lineArtMinAreaSeen, options.lineArtMinArea);
    takeInt(LINE_ART_MAX_REGIONS, options.lineArtMaxRegionsSeen, options.lineArtMaxRegions);
    takeInt(LINE_ART_ADAPTIVE_LIMIT, options.lineArtAdaptiveLimitSeen, options.lineArtAdaptiveLimit);
    takeInt(LINE_ART_MIN_ADAPTIVE_SIDE, options.lineArtMinAdaptiveSideSeen, options.lineArtMinAdaptiveSide);
    takeDouble(LINE_ART_TOLERANCE, options.lineArtToleranceSeen, options.lineArtTolerance);
    takeDouble(LINE_ART_MIN_THICKNESS, options.lineArtMinThicknessSeen, options.lineArtMinThickness);
    // 自适应开关取 0/1；其它值一律视为未给出，保持默认开启。
    {
        const QString text = takeOptionPath(arguments, LINE_ART_ADAPTIVE);
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
