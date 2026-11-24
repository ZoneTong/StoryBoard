#pragma once

#include "app/Settings.h"
#include "items/CanvasTypes.h"
#include "items/geometry/LineGlyph.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "lineart/LineArtConverter.h"
#include "ops/Alignment.h"
#include "ops/BoolOps.h"
#include "ui/widgets/LayoutMetrics.h"

#include <QColor>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QMainWindow>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>

class CanvasScene;
class CanvasView;
class CanvasItem;
class CustomShapePanel;
class QKeyEvent;

namespace selftest {
class Context;
void RunFillStyleBasicTests(Context& ctx);
void RunMainWindowShapeMenuTests(Context& ctx);
void RunDockPanelMainWindowTests(Context& ctx);
} // namespace selftest
class PropertiesPanel;
class LayerPanel;
class CanvasBackgroundController;
class DockPanelController;
class ShortcutController;
class DocumentController;
class ImportExportController;
class StyleController;
class ViewStatusController;
class UiBuilderController;
class UiActionFactory;
class UiMenuBuilder;
class UiToolBarBuilder;
class UiStatusBarBuilder;
class MainWindowEditActionsController;
class MainWindowInsertController;
class MainWindowAppController;
class FullScreenController;
class FullScreenBar;
class QAction;
class QActionGroup;
class QDockWidget;
class QDoubleSpinBox;
class QLabel;
class QMenu;
class QSpinBox;
class QToolButton;
class QUndoStack;
enum class ShapeType;

class MainWindow : public QMainWindow {
    Q_OBJECT
    // 需求 7：自定义图形面板负责插入模板，需要访问 insertCustomShape。
    friend class CustomShapePanel;

  public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 工具栏的统一尺寸：图标 20px，图标按钮 30×30，下拉框/数字框高 30px，
    // 三者对齐同一条 30px 基线。自检按这些值校验，QSS 与代码的尺寸偏差会直接失败。
    // 取值在 LayoutMetrics 统一维护；这里保留 MainWindow:: 别名供既有调用点使用。
    static constexpr int TOOL_BAR_ICON_SIZE = LayoutMetrics::TOOL_BAR_ICON_SIZE;
    static constexpr int TOOL_BAR_BUTTON_SIZE = LayoutMetrics::TOOL_BAR_BUTTON_SIZE;
    static constexpr int TOOL_BAR_CONTROL_HEIGHT = LayoutMetrics::TOOL_BAR_CONTROL_HEIGHT;
    // 画笔颜色是带下拉的分裂按钮（点色块直接调色、点箭头选预设），比纯图标按钮
    // 宽出一条箭头区，高度保持一致。
    static constexpr int TOOL_BAR_SPLIT_BUTTON_WIDTH = LayoutMetrics::TOOL_BAR_SPLIT_BUTTON_WIDTH;

    bool OpenCanvasFile(const QString& path);
    CanvasScene* Scene() const { return m_scene; }
    CanvasView* View() const { return m_view; }
    // 快捷键 / 导入导出等控制器经此读写配置，不再靠 friend 穿透私有成员。
    AppSettings& Settings() { return m_settings; }

    // 按当前语言刷新整个界面文字（设置里切换语言时调用）。
    void ApplyUiLanguage();
    // 动作可用态按选择 / 剪贴板 / 撤销栈刷新；编辑控制器与工厂经公开入口触发。
    void UpdateActions();

  protected:
    void closeEvent(QCloseEvent* event) override;
    // 全屏由窗口管理器踢出时同步界面状态（F11 之后的兜底路径）。
    void changeEvent(QEvent* event) override;
    // 窗口尺寸变化时把全屏悬浮工具栏重新摆到顶部居中。
    void resizeEvent(QResizeEvent* event) override;
    // 合并图元和弦：Ctrl+M 之后 1 秒内 U / X / I / S / F 触发对应子命令。
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private slots:
    void NewCanvas();
    void OpenCanvasDialog();
    bool SaveCanvas();
    bool SaveCanvasAs();
    void ExportCanvas();
    void ExportExcalidraw();
    void ExportCustomShape();
    void ImportImagesDialog();
    void ImportOutlineImageDialog();
    void InsertTextAt(const QPointF& scenePos);
    void ChooseBrushColor();
    void ReplaceCanvasBackgroundImage();
    void SetBackgroundColor();
    void ClearBackground();
    void ShowSettings();
    void ShowAbout();
    void ShowShortcuts();
    void SetToolMode(ToolMode mode);
    void UpdateTitle();
    void UpdateStatusBar();
    void UpdateModeUi();
    void OpenCanvasesFolder();
    // F11：进入 / 退出全屏（状态机与快照还原由 FullScreenController 负责）。
    void ToggleFullScreen();

  public:
    // 需求 7：展开自定义图形浮动面板（按目录分标签页）；anchor 为空时沿用上次位置。
    void OpenCustomShapePanel(QWidget* anchor);
    // 插入常见图形；自检需要直接驱动插入流程（含立方体 / 圆柱体的组合面插入）。
    void InsertShape(ShapeType type, qreal triangleAngle1 = -1.0, qreal triangleAngle2 = -1.0, qreal cornerRatio = 0.0);
    // 线条插入入口：直线 / 弧线 / 正弦都不是形状类型，按形态分派。
    void InsertCurve(LineGlyph glyph, ArrowStyle startArrow = ArrowStyle::NONE, ArrowStyle endArrow = ArrowStyle::NONE);
    // 需求 16：自检需要直接驱动工具栏样式取值（填充样式 / 画笔色 / 描线宽度）验证新图元生成。
    friend void selftest::RunFillStyleBasicTests(selftest::Context&);
    friend void selftest::RunMainWindowShapeMenuTests(selftest::Context&);
    // 自检需要直接驱动自定义图形导入，验证多图元自动成组与插入同一条撤销。
    friend void selftest::RunDockPanelMainWindowTests(selftest::Context&);

  private:
    void CreateActions();
    void CreateShapeMenu();
    void UpdateCustomShapeMenu();
    void CreateMenus();
    void CreateToolBars();
    void CreateStatusBar();
    void ConnectSignals();
    // 多图元对齐：菜单与工具栏下拉共用同一组动作，状态按当前选择更新。
    void ApplyAlignment(Alignment::Mode mode);
    // 合并形状：联合 / 组合 / 拆分 / 相交 / 剪除，结果替换为一个多边形，只记一条撤销。
    void ApplyBoolOperation(BoolOps::Operation operation);

    void InsertBrace(bool left);
    void InsertCustomShape(const QString& filePath);
    void OpenShapesFolder();
    void UpdatePropertiesPanelDockVisibility();
    void SetFillStyle(FillStyle style);
    void ChooseFillColor();
    void ChooseStrokeColor();
    // 描线样式（需求 1）：与填充样式同构的全局状态入口。
    void SetStrokeStyle(Qt::PenStyle style);
    // 菜单“描线宽度”普通项触发后聚焦工具栏宽度输入框末尾（需求 1）。
    void FocusStrokeWidthSpin();
    void UpdateFillActions();
    void SetBrushColor(const QColor& color, bool remember);
    void SetBrushWidthUi(int width);
    void ApplyBrushWidth(int width);
    void SetStrokeSmoothingUi(qreal tolerance);
    void UpdateCanvasesMenu();
    void UpdateBackgroundModeActions();
    void RotateSelected(qreal degrees);

  private:
    friend class CanvasBackgroundController;
    friend class DockPanelController;
    friend class DocumentController;
    friend class ImportExportController;
    friend class StyleController;
    friend class ViewStatusController;
    friend class UiBuilderController;
    friend class UiActionFactory;
    friend class UiMenuBuilder;
    friend class UiToolBarBuilder;
    friend class UiStatusBarBuilder;
    friend class MainWindowInsertController;
    friend class MainWindowAppController;
    friend class FullScreenController;
    friend class FullScreenBar;

    CanvasScene* m_scene = nullptr;
    CanvasView* m_view = nullptr;
    QUndoStack* m_undo = nullptr;
    AppSettings m_settings;
    // 图片线稿化参数的进程内副本：初始值取自配置文件，参数对话框改的就是它；
    // 只有用户点“保存到配置文件”时才把它写回 StoryBoard.ini。
    AppSettings m_lineArtSettings;
    QString m_filePath;
    QColor m_brushColor;
    QColor m_fillColor;
    // 需求 16：首次打开默认“无填充 + 灰色填充色”，描线为黑色实线 1px（见 Settings 默认值）。
    FillStyle m_fillStyle = FillStyle::NONE;
    // 描线样式与填充样式一样是全局状态：新插入图元与选中图元都读它。
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
    int m_brushWidth = 4;
    // 合并图元和弦进行中：Ctrl+M 已按下，等 1 秒内的子命令字母。
    bool m_boolChordPending = false;

    QAction* m_newAct = nullptr;
    QAction* m_openAct = nullptr;
    QAction* m_saveAct = nullptr;
    QAction* m_saveAsAct = nullptr;
    QAction* m_importAct = nullptr;
    QAction* m_lineArtAct = nullptr;
    QAction* m_exportAct = nullptr;
    QAction* m_exportExcalidrawAct = nullptr;
    QAction* m_exportCustomShapeAct = nullptr;
    QAction* m_quitAct = nullptr;

    QAction* m_undoAct = nullptr;
    QAction* m_redoAct = nullptr;
    QAction* m_deleteAct = nullptr;
    QAction* m_copyAct = nullptr;
    QAction* m_cutAct = nullptr;
    QAction* m_pasteAct = nullptr;
    QAction* m_duplicateAct = nullptr;
    QAction* m_groupAct = nullptr;
    QAction* m_ungroupAct = nullptr;
    QAction* m_smoothAct = nullptr;
    QAction* m_selectAllAct = nullptr;
    QAction* m_rotateCwAct = nullptr;
    QAction* m_rotateCcwAct = nullptr;

    QAction* m_viewModeAct = nullptr;
    QAction* m_brushModeAct = nullptr;
    QAction* m_editModeAct = nullptr;
    QAction* m_textModeAct = nullptr;
    QAction* m_fitAct = nullptr;
    QAction* m_zoomInAct = nullptr;
    QAction* m_zoomOutAct = nullptr;
    QAction* m_actualSizeAct = nullptr;
    // 视图菜单里的辅助网格开关，和「设置 → 画布」里的同名复选框共用 settings.showGrid。
    QAction* m_gridAct = nullptr;
    // F11 全屏开关：勾选态跟随实际窗口状态。
    QAction* m_fullScreenAct = nullptr;

    QAction* m_replaceBgAct = nullptr;
    QAction* m_bgColorAct = nullptr;
    QAction* m_clearBgAct = nullptr;
    QAction* m_bgStretchAct = nullptr;
    QAction* m_bgFitAct = nullptr;
    QAction* m_bgTileAct = nullptr;
    QAction* m_settingsAct = nullptr;
    QAction* m_fillColorAct = nullptr;
    QAction* m_strokeColorAct = nullptr;

    QAction* m_aboutAct = nullptr;
    QAction* m_shortcutsAct = nullptr;
    QAction* m_aboutQtAct = nullptr;
    // 对齐动作（左 / 水平居中 / 右 / 上 / 垂直居中 / 下 / 水平等距 / 垂直等距）。
    QList<QAction*> m_alignActions;
    QToolButton* m_alignButton = nullptr;
    // 布尔运算动作（联合 / 组合 / 拆分 / 相交 / 相减）。
    QList<QAction*> m_boolActions;
    QToolButton* m_boolButton = nullptr;

    QActionGroup* m_modeGroup = nullptr;
    QActionGroup* m_backgroundModeGroup = nullptr;
    QMenu* m_fileMenu = nullptr;
    // 插入菜单里的“描线样式”子菜单：样式变化时同步它的图标。
    QMenu* m_strokeStyleMenu = nullptr;
    QAction* m_boardSectionSeparator = nullptr;
    QMenu* m_shapeMenu = nullptr;
    QMenu* m_customShapeMenu = nullptr;
    CustomShapePanel* m_customShapePanel = nullptr;

    QToolButton* m_fillButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    // 描线样式按钮（需求 1）：与填充样式按钮同构，弹出菜单列出 BorderStyles()。
    QToolButton* m_strokeStyleButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    // 菜单里的“描线宽度”是普通菜单项（需求 1）：触发后把焦点放到工具栏宽度输入框末尾。
    QAction* m_strokeWidthFocusAct = nullptr;
    // 描线样式动作与分组（需求 1/15）：卡片、菜单、工具栏、属性窗共用同一候选表。
    QList<QAction*> m_strokeStyleActions;
    QActionGroup* m_strokeStyleGroup = nullptr;
    // 填充样式动作与分组：顺序与 FillStyleMeta 注册表一致。
    QList<QAction*> m_fillStyleActions;
    QActionGroup* m_fillStyleGroup = nullptr;
    QDoubleSpinBox* m_smoothingSpin = nullptr;
    std::unique_ptr<CanvasBackgroundController> m_background;
    std::unique_ptr<DockPanelController> m_docks;
    std::unique_ptr<ShortcutController> m_shortcuts;
    std::unique_ptr<DocumentController> m_document;
    std::unique_ptr<ImportExportController> m_io;
    std::unique_ptr<StyleController> m_style;
    std::unique_ptr<ViewStatusController> m_viewStatus;
    std::unique_ptr<UiBuilderController> m_ui;
    std::unique_ptr<MainWindowEditActionsController> m_editActions;
    std::unique_ptr<MainWindowInsertController> m_insert;
    std::unique_ptr<MainWindowAppController> m_app;
    std::unique_ptr<FullScreenController> m_fullScreen;

    QLabel* m_hintLabel = nullptr;
    QLabel* m_modeBadge = nullptr;
    QLabel* m_zoomBadge = nullptr;
    QLabel* m_countBadge = nullptr;
};
