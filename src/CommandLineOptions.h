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
    bool aboutScreenshot = false;
    bool itemPropertiesScreenshot = false;
    bool shapeMenuScreenshot = false;
    bool brushStyleMenuScreenshot = false;
    bool brushWidthMenuScreenshot = false;
    bool fillStyleMenuScreenshot = false;
    bool styleMenuScreenshot = false;
    bool exportScreenshot = false;

    QString selfTestPath;
    QString renderDemoPath;
    QString demoCanvasPath;
    QString uiScreenshotPath;
    QString settingsScreenshotPath;
    QString setLanguageValue;
    QString settingsProbePath;
    QString aboutScreenshotPath;
    QString itemPropertiesScreenshotPath;
    QString shapeMenuScreenshotPath;
    QString brushStyleMenuScreenshotPath;
    QString brushWidthMenuScreenshotPath;
    QString fillStyleMenuScreenshotPath;
    QString styleMenuScreenshotPath;
    QString exportScreenshotPath;

    // 位置参数里第一个存在的 .sbcanvas 文件路径（可空）。
    QString canvasPath;

  private:
    CommandLineOptions() = default;

    static QString takeOptionPath(const QStringList& arguments, const QString& option);
};
