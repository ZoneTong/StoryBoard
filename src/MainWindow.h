#pragma once

#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasTypes.h"
#include "LineArtConverter.h"
#include "LineGlyph.h"
#include "Settings.h"
#include "ShapeItem.h"

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
void runFillStyleBasicTests(Context& ctx);
void runMainWindowShapeMenuTests(Context& ctx);
} // namespace selftest
class ItemPropertiesDialog;
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
    static constexpr int TOOL_BAR_ICON_SIZE = 20;
    static constexpr int TOOL_BAR_BUTTON_SIZE = 30;
    static constexpr int TOOL_BAR_CONTROL_HEIGHT = 30;
    // 画笔颜色是带下拉的分裂按钮（点色块直接调色、点箭头选预设），比纯图标按钮
    // 宽出一条箭头区，高度保持一致。
    static constexpr int TOOL_BAR_SPLIT_BUTTON_WIDTH = 34;

    bool openCanvasFile(const QString& path);
    CanvasScene* scene() const { return m_scene; }
    CanvasView* view() const { return m_view; }

    // 按当前语言刷新整个界面文字（设置里切换语言时调用）。
    void applyUiLanguage();

  protected:
    void closeEvent(QCloseEvent* event) override;
    // 合并图元和弦：Ctrl+M 之后 1 秒内 U / X / I / S / F 触发对应子命令。
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private slots:
    void newCanvas();
    void openCanvasDialog();
    bool saveCanvas();
    bool saveCanvasAs();
    void exportCanvas();
    void exportExcalidraw();
    void exportCustomShape();
    void importImagesDialog();
    void importOutlineImageDialog();
    void insertTextAt(const QPointF& scenePos);
    void chooseBrushColor();
    void replaceCanvasBackgroundImage();
    void setBackgroundColor();
    void clearBackground();
    void showSettings();
    void showAbout();
    void showShortcuts();
    void setToolMode(ToolMode mode);
    void updateTitle();
    void updateActions();
    void updateStatusBar();
    void updateModeUi();
    void openCanvasesFolder();

  public:
    // 需求 7：展开自定义图形浮动面板（按目录分标签页）；anchor 为空时沿用上次位置。
    void openCustomShapePanel(QWidget* anchor);
    // 插入常见图形；自检需要直接驱动插入流程（含立方体 / 圆柱体的组合面插入）。
    // parallelogramAngle > 0 表示菱形预置：平行四边形锁该内角并旋转 45° 插入。
    void insertShape(ShapeType type, qreal triangleAngle1 = -1.0, qreal triangleAngle2 = -1.0, qreal cornerRatio = 0.0,
                     qreal parallelogramAngle = 0.0);
    // 线条插入入口：直线 / 弧线 / 正弦都不是形状类型，按形态分派。
    void insertCurve(LineGlyph glyph, ArrowStyle startArrow = ArrowStyle::NONE, ArrowStyle endArrow = ArrowStyle::NONE);
    // 需求 16：自检需要直接驱动工具栏样式取值（填充样式 / 画笔色 / 描线宽度）验证新图元生成。
    friend void selftest::runFillStyleBasicTests(selftest::Context&);
    friend void selftest::runMainWindowShapeMenuTests(selftest::Context&);

  private:
    void createActions();
    void createShapeMenu();
    void updateCustomShapeMenu();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void connectSignals();
    // 多图元对齐：菜单与工具栏下拉共用同一组动作，状态按当前选择更新。
    void applyAlignment(Alignment::Mode mode);
    // 合并形状：联合 / 组合 / 拆分 / 相交 / 剪除，结果替换为一个多边形，只记一条撤销。
    void applyBoolOperation(BoolOps::Operation operation);

    void insertBrace(bool left);
    void insertCustomShape(const QString& filePath);
    void openShapesFolder();
    void updatePropertiesPanelDockVisibility();
    void SetFillStyle(FillStyle style);
    void chooseFillColor();
    void chooseStrokeColor();
    // 描线样式（需求 1）：与填充样式同构的全局状态入口。
    void SetStrokeStyle(Qt::PenStyle style);
    // 菜单“描线宽度”普通项触发后聚焦工具栏宽度输入框末尾（需求 1）。
    void focusStrokeWidthSpin();
    void updateFillActions();
    void setBrushColor(const QColor& color, bool remember);
    void setBrushWidthUi(int width);
    void applyBrushWidth(int width);
    void setStrokeSmoothingUi(qreal tolerance);
    void updateCanvasesMenu();
    void updateBackgroundModeActions();
    void showSelectedItemProperties();
    void showItemProperties(CanvasItem* item);
    void rotateSelected(qreal degrees);
    void closeItemProperties();

  private:
    friend class CanvasBackgroundController;
    friend class DockPanelController;
    friend class ShortcutController;
    friend class DocumentController;
    friend class ImportExportController;
    friend class StyleController;
    friend class ViewStatusController;
    friend class UiBuilderController;
    friend class UiActionFactory;
    friend class UiMenuBuilder;
    friend class UiToolBarBuilder;
    friend class UiStatusBarBuilder;
    friend class MainWindowEditActionsController;
    friend class MainWindowInsertController;
    friend class MainWindowAppController;

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
    QAction* m_itemPropsAct = nullptr;
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

    QAction* m_replaceBgAct = nullptr;
    QAction* m_bgColorAct = nullptr;
    QAction* m_clearBgAct = nullptr;
    QAction* m_bgStretchAct = nullptr;
    QAction* m_bgFitAct = nullptr;
    QAction* m_bgTileAct = nullptr;
    QAction* m_settingsAct = nullptr;
    QAction* m_fillNoneAct = nullptr;
    QAction* m_fillSolidAct = nullptr;
    QAction* m_fillHachureAct = nullptr;
    QAction* m_fillReverseAct = nullptr;
    QAction* m_fillCrossAct = nullptr;
    QAction* m_fillGridAct = nullptr;
    QAction* m_fillHoneycombAct = nullptr;
    QAction* m_fillHorizontalAct = nullptr;
    QAction* m_fillVerticalAct = nullptr;
    QAction* m_fillBrickAct = nullptr;
    QAction* m_fillDotsAct = nullptr;
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
    QActionGroup* m_fillStyleGroup = nullptr;
    QMenu* m_fileMenu = nullptr;
    // 插入菜单里的“描线样式”子菜单：样式变化时同步它的图标。
    QMenu* m_strokeStyleMenu = nullptr;
    QAction* m_boardSectionSeparator = nullptr;
    QMenu* m_shapeMenu = nullptr;
    QMenu* m_customShapeMenu = nullptr;
    CustomShapePanel* m_customShapePanel = nullptr;

    QToolButton* m_fillButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    // 描线样式按钮（需求 1）：与填充样式按钮同构，弹出菜单列出 borderStyles()。
    QToolButton* m_strokeStyleButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    // 菜单里的“描线宽度”是普通菜单项（需求 1）：触发后把焦点放到工具栏宽度输入框末尾。
    QAction* m_strokeWidthFocusAct = nullptr;
    // 描线样式动作与分组（需求 1/15）：卡片、菜单、工具栏、属性窗共用同一候选表。
    QList<QAction*> m_strokeStyleActions;
    QActionGroup* m_strokeStyleGroup = nullptr;
    QDoubleSpinBox* m_smoothingSpin = nullptr;
    QPointer<ItemPropertiesDialog> m_itemPropertiesDialog;
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

    QLabel* m_hintLabel = nullptr;
    QLabel* m_modeBadge = nullptr;
    QLabel* m_zoomBadge = nullptr;
    QLabel* m_countBadge = nullptr;
};
