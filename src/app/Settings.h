#pragma once

#include "app/LogLevel.h"

#include <QByteArray>
#include <QColor>
#include <QMap>
#include <QSizeF>
#include <QString>
#include <QStringList>

class AppSettings {
  public:
    // 需求 16：首次运行的默认描线为“黑色实线 1px”（之后按用户在设置里改过的值记忆）。
    QColor brushColor = QColor(0, 0, 0);
    int brushWidth = 1;
    // 画笔拟合容差，单位为画布像素，支持 0.1 精度；0 表示关闭拟合。
    qreal strokeSmoothing = 2.0;
    // 默认画布 48×32 个棋盘格（棋盘格边长 32 逻辑像素），宽高都是 32 的整数倍。
    QSizeF canvasSize = QSizeF(1536.0, 1024.0);
    // 初始设置默认不显示辅助网格，需要时从视图菜单或设置里手动打开。
    bool showGrid = false;
    QColor canvasColor = QColor(43, 47, 54);
    QString lastExportDir;
    // 导出弹窗里「自适应尺寸」的上次勾选状态；跨会话记忆，默认不勾。
    bool exportFitViewSize = false;
    QString lastImageDir;
    QString lastCanvasPath;
    QString canvasesDirectory;
    QByteArray windowGeometry;
    // 右侧卡片的上下顺序（QDockWidget 的 objectName：视图模式卡 / 图层卡）。
    // 编辑面板的位置单独记录，右侧两张卡由用户拖拽换位后自动写回，启动时恢复。
    QStringList dockOrder;
    // 编辑面板的位置。首次启动没有保存过位置时停在窗口左侧；
    // 用户拖动或悬浮后写入这里，下次启动按保存的区域 / 悬浮几何恢复。
    bool propertiesDockStateSaved = false;
    bool propertiesDockFloating = false;
    QString propertiesDockArea = QStringLiteral("left");
    QByteArray propertiesDockGeometry;
    // 自定义快捷键：键是动作的默认组合键（便携文本），值是新组合键；
    // 没有条目就用代码里的默认值。
    QMap<QString, QString> shortcutOverrides;

    // 日志详细程度：默认只落错误与告警，成功的画布操作与图元事件默认不打印。
    LogLevel logLevel = LogLevel::LOG_WARNING;

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
    void Load();
    // 把当前值合并写回配置文件，未识别的键（例如语言）保持不动。
    bool Save() const;
    // 只把图片线稿化参数写回配置文件，其余键沿用文件里的现值。
    // 线稿化参数对话框默认只改内存，只有用户点“保存到配置文件”时才调用它。
    bool SaveLineArt() const;
    // 只把另一份设置里的图片线稿化参数拷过来，其它字段不动。
    void CopyLineArtFrom(const AppSettings& other);

    // 唯一的配置文件：用户配置目录下的 StoryBoard.ini；进程内每次返回同一路径。
    static QString SettingsFilePath();
    static QString DefaultCanvasesDir();
    static QString CanvasesDir();
    static void EnsureCanvasesDir();
    // 自定义图形模板目录：与 StoryBoard.ini 同级，固定叫 shapes。
    // 未保存改动临时备份目录：配置目录同级的 recovery/（便携版跟着配置走）。
    static QString RecoveryDir();
    static void EnsureRecoveryDir();
    static QString ShapesDir();
    // 程序部署目录（可执行文件所在目录）下的 shapes 子目录，便携版可直接往里放模板。
    static QString BundledShapesDir();
    // 参与扫描的模板目录，顺序即优先顺序：先配置文件同级，再部署目录；
    // 同名模板取前面的那份。两者不存在时不建目录，避免在只读安装目录里写入。
    static QStringList ShapesDirs();
    static void EnsureShapesDir();
    // 可写的模板目录：配置文件同级 → 部署目录旁 → 用户配置目录，取第一个「能建且可写」的。
    // 安装到 Program Files 等只读位置时，配置同级目录建不出来，这里会自动降级；
    // 三个候选全都不可写时返回空串，调用方据此给出明确失败原因，而不是去开一个不存在的目录。
    static QString WritableShapesDir();
    // 打开模板目录的失败原因：都按「找不到目录 / 建不出来 / 外壳打不开 / 没有文件管理器」分开报。
    enum class ShapesFolderError { NONE = 0, CREATE_FAILED, SHELL_FAILED, DESKTOP_FAILED };
    struct ShapesFolderOpenResult {
        bool opened = false;
        ShapesFolderError error = ShapesFolderError::NONE;
        QString directory;
        int shellCode = 0;
        // 诊断用：进程完整性级别与各外壳通道的返回值，排查「打不开目录」时直接看现场。
        QString diagnostics;
    };
    // 在系统文件管理器里打开自定义图形目录；目录不存在时先在可写位置创建。
    static ShapesFolderOpenResult OpenShapesFolder();
    // 失败文案：带目录路径与原因，面板按钮与菜单项共用同一句，避免两处说法不一样。
    static QString ShapesFolderFailureText(const ShapesFolderOpenResult& result);
    // 界面语言以 "system" / "zh" / "en" 存进同一个 ini，不再使用注册表。
    static QString LanguageSetting();
    static bool SetLanguageSetting(const QString& stored);
};
