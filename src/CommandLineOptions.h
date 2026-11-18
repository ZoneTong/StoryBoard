#pragma once

#include <QString>
#include <QStringList>

// 解析命令行参数：识别各 -- 开关及其附带的取值 / 输出路径。
// 纯数据解析，不触碰环境变量、QApplication 或主窗口。
class CommandLineOptions {
  public:
    static CommandLineOptions parse(const QStringList& arguments);

    bool selfTest = false;
    bool renderDemo = false;
    bool demoCanvas = false;
    bool uiScreenshot = false;
    bool settingsScreenshot = false;
    bool settingsApplyProbe = false;
    bool setLanguage = false;
    bool settingsProbe = false;
    // 诊断：调用一次 AppSettings::openShapesFolder 并把结果写到指定文件。
    bool openShapesProbe = false;
    bool aboutScreenshot = false;
    bool itemPropertiesScreenshot = false;
    bool shapeMenuScreenshot = false;
    bool brushStyleMenuScreenshot = false;
    bool brushWidthMenuScreenshot = false;
    bool fillStyleMenuScreenshot = false;
    bool styleMenuScreenshot = false;
    bool exportScreenshot = false;
    // 命令行线稿化：把图片按色块轮廓转成曲线图元并保存为 .sbcanvas 画布。
    bool lineArt = false;

    QString selfTestPath;
    QString renderDemoPath;
    QString demoCanvasPath;
    QString uiScreenshotPath;
    QString settingsScreenshotPath;
    QString setLanguageValue;
    QString settingsProbePath;
    QString openShapesProbePath;
    QString aboutScreenshotPath;
    QString itemPropertiesScreenshotPath;
    QString shapeMenuScreenshotPath;
    QString brushStyleMenuScreenshotPath;
    QString brushWidthMenuScreenshotPath;
    QString fillStyleMenuScreenshotPath;
    QString styleMenuScreenshotPath;
    QString exportScreenshotPath;
    QString lineArtInputPath;
    QString lineArtOutputPath;

    // 线稿化档位：0 保留细节 / 1 均衡（默认）/ 2 干净线条 / 3 粗块面。
    // 显式给出的细项覆盖档位里的对应值；最细条带默认 1px。
    int lineArtPreset = 1;
    bool lineArtMaxSideSeen = false;
    int lineArtMaxSide = 0;
    bool lineArtColorLevelsSeen = false;
    int lineArtColorLevels = 0;
    bool lineArtPaletteColorsSeen = false;
    int lineArtPaletteColors = 0;
    bool lineArtPaletteMergeSeen = false;
    double lineArtPaletteMerge = 0.0;
    bool lineArtAdjacentMergeSeen = false;
    double lineArtAdjacentMerge = 0.0;
    bool lineArtMinAreaSeen = false;
    int lineArtMinArea = 0;
    bool lineArtMaxRegionsSeen = false;
    int lineArtMaxRegions = 0;
    bool lineArtAdaptiveSeen = false;
    bool lineArtAdaptive = true;
    bool lineArtAdaptiveLimitSeen = false;
    int lineArtAdaptiveLimit = 0;
    bool lineArtMinAdaptiveSideSeen = false;
    int lineArtMinAdaptiveSide = 0;
    bool lineArtToleranceSeen = false;
    double lineArtTolerance = 0.0;
    bool lineArtMinThicknessSeen = false;
    double lineArtMinThickness = 0.0;

    // 位置参数里第一个存在的 .sbcanvas 文件路径（可空）。
    QString canvasPath;

  private:
    CommandLineOptions() = default;

    static QString takeOptionPath(const QStringList& arguments, const QString& option);
};
