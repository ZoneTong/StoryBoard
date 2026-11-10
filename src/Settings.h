#pragma once

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

    void load();
    void save() const;

    static QString defaultCanvasesDir();
    static QString canvasesDir();
    static void ensureCanvasesDir();
};
