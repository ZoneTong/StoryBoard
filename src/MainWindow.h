#pragma once

#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasTypes.h"
#include "LineArtConverter.h"
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
class ItemPropertiesDialog;
class ItemDockPanel;
class LayerPanel;
class BackgroundController;
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
  public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 工具栏的统一尺寸：图标 20px，图标按钮 30×30，下拉框/数字框高 30px，
    // 三者对齐同一条 30px 基线。自检按这些值校验，QSS 与代码的尺寸偏差会直接失败。
    static constexpr int kToolBarIconSize = 20;
    static constexpr int kToolBarButtonSize = 30;
    static constexpr int kToolBarControlHeight = 30;
    // 画笔颜色是带下拉的分裂按钮（点色块直接调色、点箭头选预设），比纯图标按钮
    // 宽出一条箭头区，高度保持一致。
    static constexpr int kToolBarSplitButtonWidth = 34;

    bool openCanvasFile(const QString& path);
    CanvasScene* scene() const { return m_scene; }
    CanvasView* view() const { return m_view; }

    // 按当前语言刷新整个界面文字（设置里切换语言时调用）。
    void applyUiLanguage();

  protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private slots:
    void newCanvas();
    void openCanvasDialog();
    bool saveCanvas();
    bool saveCanvasAs();
    void exportCanvas();
    void importImagesDialog();
    void importOutlineImageDialog();
    void insertTextAt(const QPointF& scenePos);
    void chooseBrushColor();
    void replaceBackgroundImage();
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

    void insertShape(ShapeType type);
    void insertCustomShape(const QString& filePath);
    void openShapesFolder();
    void updateItemDockVisibility();
    void setFillStyle(FillStyle style);
    void chooseFillColor();
    void chooseStrokeColor();
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
    friend class BackgroundController;
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
    FillStyle m_fillStyle = FillStyle::Solid;
    int m_brushWidth = 4;

    QAction* m_newAct = nullptr;
    QAction* m_openAct = nullptr;
    QAction* m_saveAct = nullptr;
    QAction* m_saveAsAct = nullptr;
    QAction* m_importAct = nullptr;
    QAction* m_lineArtAct = nullptr;
    QAction* m_exportAct = nullptr;
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
    QAction* m_boardSectionSeparator = nullptr;
    QMenu* m_shapeMenu = nullptr;
    QMenu* m_customShapeMenu = nullptr;

    QToolButton* m_fillButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_menuWidthSpin = nullptr;
    QDoubleSpinBox* m_smoothingSpin = nullptr;
    QPointer<ItemPropertiesDialog> m_itemPropertiesDialog;
    std::unique_ptr<BackgroundController> m_background;
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
