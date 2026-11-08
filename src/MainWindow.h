#pragma once

#include <QColor>
#include <QMainWindow>
#include <QPointF>
#include <QString>

#include "BoardTypes.h"
#include "Settings.h"
#include "ShapeItem.h"

class BoardScene;
class BoardView;
class BoardItem;
class LayerPanel;
class QAction;
class QActionGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QToolButton;
class QUndoStack;
class TextItem;
enum class ShapeType;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openTemplateFile(const QString& path);
    BoardScene* scene() const { return m_scene; }
    BoardView* view() const { return m_view; }

    // 按当前语言刷新整个界面文字（设置里切换语言时调用）。
    void applyUiLanguage();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void newBoard();
    void openTemplateDialog();
    bool saveTemplate();
    bool saveTemplateAs();
    void exportBoard();
    void importImagesDialog();
    void insertTextAt(const QPointF& scenePos);
    void editTextItem(TextItem* item);
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
    void openTemplatesFolder();

private:
    void createActions();
    void createShapeMenu();
    void createLayerDock();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void connectSignals();

    bool maybeSave();
    bool loadTemplateFile(const QString& path, bool showErrors);
    bool saveToFile(const QString& path);
    bool exportToFile(const QString& path, const QRectF& area, qreal scale, const QString& format,
                      bool showCompletion = true, int layerId = -1);
    void importImageFiles(const QStringList& files);
    void insertShape(ShapeType type);
    void setFillStyle(FillStyle style);
    void chooseFillColor();
    void chooseStrokeColor();
    void applyFillToSelection();
    void updateFillActions();
    void setBrushColor(const QColor& color, bool remember);
    void setBrushWidthUi(int width);
    void updateTemplateMenu();
    void updateBackgroundModeActions();
    void showSelectedItemProperties();
    void showItemProperties(BoardItem* item);
    QString documentName() const;

    BoardScene* m_scene = nullptr;
    BoardView* m_view = nullptr;
    QUndoStack* m_undo = nullptr;
    AppSettings m_settings;
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
    QAction* m_exportAct = nullptr;
    QAction* m_quitAct = nullptr;

    QAction* m_undoAct = nullptr;
    QAction* m_redoAct = nullptr;
    QAction* m_deleteAct = nullptr;
    QAction* m_selectAllAct = nullptr;
    QAction* m_itemPropsAct = nullptr;

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
    QMenu* m_templatesMenu = nullptr;
    QMenu* m_shapeMenu = nullptr;

    QDockWidget* m_layerDock = nullptr;
    LayerPanel* m_layerPanel = nullptr;

    QToolButton* m_colorButton = nullptr;
    QToolButton* m_fillButton = nullptr;
    QToolButton* m_fillColorButton = nullptr;
    QToolButton* m_strokeColorButton = nullptr;
    QComboBox* m_widthCombo = nullptr;

    QLabel* m_hintLabel = nullptr;
    QLabel* m_modeBadge = nullptr;
    QLabel* m_zoomBadge = nullptr;
    QLabel* m_countBadge = nullptr;
};
