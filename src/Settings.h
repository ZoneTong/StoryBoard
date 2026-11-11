#pragma once

#include <QByteArray>
#include <QColor>
#include <QSizeF>
#include <QString>

class AppSettings
{
public:
    QColor brushColor = QColor(30, 41, 59);
    int brushWidth = 4;
    // 画笔收尾自动平滑容差，单位为画布像素；0 表示关闭自动平滑。
    int strokeSmoothing = 4;
    QSizeF canvasSize = QSizeF(1920.0, 1080.0);
    bool showGrid = true;
    QColor canvasColor = QColor(43, 47, 54);
    QString lastExportDir;
    QString lastImageDir;
    QString lastCanvasPath;
    QString canvasesDirectory;
    QByteArray windowGeometry;

    // 图片线稿化参数，见 LineArtConverter::Options。用户可在设置里调整。
    int lineArtColorLevels = 6;
    int lineArtPaletteMerge = 48;
    int lineArtAdjacentMerge = 48;
    int lineArtMinRegionArea = 12;
    // 色块过多时自动降低处理尺寸（默认开启），超过 lineArtAdaptiveLimit 块就触发。
    bool lineArtAdaptiveDownscale = true;
    // 轮廓简化容差与最细条带阈值，单位都是处理尺度像素。
    qreal lineArtSimplifyTolerance = 0.6;
    qreal lineArtMinThickness = 1.0;
    int lineArtAdaptiveLimit = 2000;

    // 从配置文件读取；配置文件不存在时按默认值写出一份，保证首次运行就有文件。
    void load();
    // 把当前值合并写回配置文件，未识别的键（例如语言）保持不动。
    bool save() const;

    // 唯一的配置文件：用户配置目录下的 StoryBoard.ini；进程内每次返回同一路径。
    static QString settingsFilePath();
    static QString defaultCanvasesDir();
    static QString canvasesDir();
    static void ensureCanvasesDir();

    // 界面语言以 "system" / "zh" / "en" 存进同一个 ini，不再使用注册表。
    static QString languageSetting();
    static bool setLanguageSetting(const QString& stored);
};
