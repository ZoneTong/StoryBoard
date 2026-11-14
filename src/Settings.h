#pragma once

#include <QByteArray>
#include <QColor>
#include <QMap>
#include <QSizeF>
#include <QString>
#include <QStringList>

class AppSettings {
  public:
    QColor brushColor = QColor(30, 41, 59);
    int brushWidth = 4;
    // 画笔收尾自动平滑容差，单位为画布像素；0 表示关闭自动平滑。
    int strokeSmoothing = 4;
    QSizeF canvasSize = QSizeF(1920.0, 1080.0);
    // 初始设置默认不显示辅助网格，需要时从视图菜单或设置里手动打开。
    bool showGrid = false;
    QColor canvasColor = QColor(43, 47, 54);
    QString lastExportDir;
    QString lastImageDir;
    QString lastCanvasPath;
    QString canvasesDirectory;
    QByteArray windowGeometry;
    // 右侧卡片的上下顺序（QDockWidget 的 objectName：视图模式卡 / 图层卡）。
    // 编辑图元卡固定在左侧；右侧两张卡由用户拖拽换位后自动写回，启动时恢复。
    QStringList dockOrder;
    // 自定义快捷键：键是动作的默认组合键（便携文本），值是新组合键；
    // 没有条目就用代码里的默认值。
    QMap<QString, QString> shortcutOverrides;

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
    // 只把图片线稿化参数写回配置文件，其余键沿用文件里的现值。
    // 线稿化参数对话框默认只改内存，只有用户点“保存到配置文件”时才调用它。
    bool saveLineArt() const;
    // 只把另一份设置里的图片线稿化参数拷过来，其它字段不动。
    void copyLineArtFrom(const AppSettings& other);

    // 唯一的配置文件：用户配置目录下的 StoryBoard.ini；进程内每次返回同一路径。
    static QString settingsFilePath();
    static QString defaultCanvasesDir();
    static QString canvasesDir();
    static void ensureCanvasesDir();
    // 自定义图形模板目录：与 StoryBoard.ini 同级，固定叫 shapes。
    static QString shapesDir();
    // 程序部署目录（可执行文件所在目录）下的 shapes 子目录，便携版可直接往里放模板。
    static QString bundledShapesDir();
    // 参与扫描的模板目录，顺序即优先顺序：先配置文件同级，再部署目录；
    // 同名模板取前面的那份。两者不存在时不建目录，避免在只读安装目录里写入。
    static QStringList shapesDirs();
    static void ensureShapesDir();
    // 界面语言以 "system" / "zh" / "en" 存进同一个 ini，不再使用注册表。
    static QString languageSetting();
    static bool setLanguageSetting(const QString& stored);
};
