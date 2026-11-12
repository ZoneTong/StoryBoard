#pragma once

#include "CanvasTypes.h"
#include "LineArtConverter.h"
#include "Settings.h"
#include "ShapeItem.h"

#include <QColor>
#include <QMainWindow>
#include <QPointF>
#include <QPointer>
#include <QString>

class CanvasScene;
class CanvasView;
class CanvasItem;
class ItemPropertiesDialog;
class LayerPanel;
class QAction;
class QActionGroup;
class QComboBox;
class QDockWidget;
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
    void fitAll();
    void setToolMode(ToolMode mode);
    void updateTitle();
    void updateActions();
    void updateStatusBar();
    void updateModeUi();
    void openCanvasesFolder();

  private:
    void createActions();
    void createShapeMenu();
    void createLayerDock();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void connectSignals();
    void CreateBrushToolbar(QToolBar* mainBar);
    void CreateFillToolbar(QToolBar* mainBar);

    bool maybeSave();
    bool loadCanvasFile(const QString& path, bool showErrors);
    bool saveToFile(const QString& path);
    bool exportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                      bool showCompletion = true, int layerId = -1);
    void importImageFiles(const QStringList& files);
    void importOutlineImageFile(const QString& file, const LineArtConverter::Options& options);
    void insertShape(ShapeType type);
    void setFillStyle(FillStyle style);
    void chooseFillColor();
    void chooseStrokeColor();
    void applyFillToSelection();
    void updateFillActions();
    void setBrushColor(const QColor& color, bool remember);
    void setBrushWidthUi(int width);
    void setStrokeSmoothingUi(int tolerance);
    void updateCanvasesMenu();
    void updateBackgroundModeActions();
    void showSelectedItemProperties();
    void showItemProperties(CanvasItem* item);
    void rotateSelected(qreal degrees);
    void closeItemProperties();
    QString documentName() const;

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
    QAction* m_brushColorAct = nullptr;

    QAction* m_aboutAct = nullptr;
    QAction* m_aboutQtAct = nullptr;

    QActionGroup* m_modeGroup = nullptr;
    QActionGroup* m_backgroundModeGroup = nullptr;
    QActionGroup* m_fillStyleGroup = nullptr;
    QActionGroup* m_brushWidthGroup = nullptr;
    QMenu* m_canvasesMenu = nullptr;
    QMenu* m_shapeMenu = nullptr;

    QDockWidget* m_layerDock = nullptr;
    LayerPanel* m_layerPanel = nullptr;

    QToolButton* m_colorButton = nullptr;
    QToolButton* m_fillButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QComboBox* m_widthCombo = nullptr;
    QSpinBox* m_smoothingSpin = nullptr;
    QPointer<ItemPropertiesDialog> m_itemPropertiesDialog;

    QLabel* m_hintLabel = nullptr;
    QLabel* m_modeBadge = nullptr;
    QLabel* m_zoomBadge = nullptr;
    QLabel* m_countBadge = nullptr;
};
