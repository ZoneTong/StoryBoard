#include "UiBuilderController.h"

#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "CustomShapeTemplates.h"
#include "DockPanelController.h"
#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "MainWindow.h"
#include "ShapeItem.h"
#include "StyleController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QPair>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWidget>
#include <utility>

namespace {

QAction* makeAction(QObject* parent, IconId icon, const QString& text, const QKeySequence& shortcut = QKeySequence(),
                    QString tip = QString())
{
    // 工具栏按钮与菜单项共用一份图标，按工具栏图标尺寸生成；菜单再缩放到 16px。
    auto* action = new QAction(makeIcon(icon, QColor(58, 64, 80), MainWindow::kToolBarIconSize), text, parent);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    // 提示里统一带上快捷键，省得逐个动作手写；已有的 “(Ctrl+X)” 不会重复追加。
    // 基串记到属性里，改键之后刷新提示可以直接拿它重拼，不会把旧键留在文字里。
    action->setProperty("storyBoardBaseTip", tip);
    if (!tip.isEmpty() && !shortcut.isEmpty()) {
        const QString keys = shortcut.toString(QKeySequence::NativeText);
        if (!tip.contains(keys, Qt::CaseInsensitive)) {
            tip = tip + QStringLiteral(" (") + keys + QStringLiteral(")");
        }
    }
    action->setToolTip(tip.isEmpty() ? text : tip);
    action->setStatusTip(tip.isEmpty() ? text : tip);
    return action;
}

QLabel* makeBadge(const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QList<int> brushWidthPresets()
{
    return {1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
}

} // namespace

UiBuilderController::UiBuilderController(MainWindow* window) : m_window(window) {}

void UiBuilderController::buildActions()
{
    m_window->m_newAct =
        makeAction(m_window, IconId::New, MainWindow::tr("New canvas"), QKeySequence(QStringLiteral("Ctrl+N")),
                   MainWindow::tr("Create an empty canvas (Ctrl+N)"));
    m_window->m_openAct =
        makeAction(m_window, IconId::Open, MainWindow::tr("Open canvas…"), QKeySequence(QStringLiteral("Ctrl+O")),
                   MainWindow::tr("Open a StoryBoard canvas (Ctrl+O)"));
    m_window->m_saveAct =
        makeAction(m_window, IconId::Save, MainWindow::tr("Save canvas"), QKeySequence(QStringLiteral("Ctrl+S")),
                   MainWindow::tr("Save the current canvas (Ctrl+S)"));
    m_window->m_saveAsAct = makeAction(m_window, IconId::Save, MainWindow::tr("Save canvas as…"),
                                       QKeySequence(QStringLiteral("Ctrl+Shift+S")),
                                       MainWindow::tr("Save the current canvas to a new file (Ctrl+Shift+S)"));
    m_window->m_importAct =
        makeAction(m_window, IconId::Image, MainWindow::tr("Insert images…"), QKeySequence(QStringLiteral("Ctrl+I")),
                   MainWindow::tr("Insert images as movable, resizable items (Ctrl+I)"));
    m_window->m_lineArtAct = makeAction(
        m_window, IconId::LineArt, MainWindow::tr("Import outline image"), QKeySequence(QStringLiteral("Ctrl+Shift+I")),
        MainWindow::tr(
            "Trace an image's color-block outlines into strokes (line width 1); a parameter dialog opens first and "
            "only Save to configuration file writes StoryBoard.ini (Ctrl+Shift+I)"));
    m_window->m_exportAct =
        makeAction(m_window, IconId::Export, MainWindow::tr("Export canvas…"), QKeySequence(QStringLiteral("Ctrl+E")),
                   MainWindow::tr("Export as PNG / JPEG / PDF (Ctrl+E)"));
    m_window->m_quitAct = new QAction(MainWindow::tr("Quit"), m_window);
    m_window->m_quitAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    m_window->m_quitAct->setStatusTip(MainWindow::tr("Quit StoryBoard (Ctrl+Q)"));

    m_window->m_undoAct = makeAction(m_window, IconId::Undo, MainWindow::tr("Undo"), QKeySequence::Undo,
                                     MainWindow::tr("Undo the last step (Ctrl+Z)"));
    m_window->m_undoAct->setEnabled(false);
    m_window->m_redoAct = makeAction(m_window, IconId::Redo, MainWindow::tr("Redo"),
                                     QKeySequence(QStringLiteral("Ctrl+Y")), MainWindow::tr("Redo (Ctrl+Y)"));
    m_window->m_redoAct->setEnabled(false);

    m_window->m_deleteAct = makeAction(m_window, IconId::Delete, MainWindow::tr("Delete selected items"),
                                       QKeySequence::Delete, MainWindow::tr("Delete the selected items (Delete)"));
    m_window->m_duplicateAct = new QAction(MainWindow::tr("Duplicate selected items"), m_window);
    m_window->m_duplicateAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    m_window->m_duplicateAct->setStatusTip(MainWindow::tr("Duplicate the selected items with a small offset (Ctrl+D)"));
    m_window->m_duplicateAct->setEnabled(false);
    QObject::connect(m_window->m_duplicateAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene) {
            return;
        }
        const int duplicated = m_window->m_scene->duplicateSelectedItems();
        if (duplicated > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Duplicated %1 item(s)").arg(duplicated), 3000);
        }
    });
    m_window->m_groupAct = new QAction(MainWindow::tr("Group selected items"), m_window);
    m_window->m_groupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    m_window->m_groupAct->setStatusTip(MainWindow::tr("Group the selected items so they move together (Ctrl+G)"));
    m_window->m_groupAct->setEnabled(false);
    QObject::connect(m_window->m_groupAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene) {
            return;
        }
        const int grouped = m_window->m_scene->groupSelectedItems();
        if (grouped > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Grouped %1 item(s)").arg(grouped), 3000);
        }
    });
    m_window->m_ungroupAct = new QAction(MainWindow::tr("Ungroup selected items"), m_window);
    m_window->m_ungroupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    m_window->m_ungroupAct->setStatusTip(MainWindow::tr("Ungroup the selected group(s) (Ctrl+Shift+G)"));
    m_window->m_ungroupAct->setEnabled(false);
    QObject::connect(m_window->m_ungroupAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene) {
            return;
        }
        const int ungrouped = m_window->m_scene->ungroupSelectedItems();
        if (ungrouped > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Ungrouped %1 item(s)").arg(ungrouped), 3000);
        }
    });
    m_window->m_smoothAct = makeAction(
        m_window, IconId::Smooth, MainWindow::tr("Smooth strokes"), QKeySequence(QStringLiteral("Ctrl+Shift+M")),
        MainWindow::tr("Smooth strokes once using the current tolerance (Ctrl+Shift+M)"));
    m_window->m_smoothAct->setEnabled(false);
    QObject::connect(m_window->m_smoothAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene || !m_window->m_view) {
            return;
        }
        const int smoothed = m_window->m_scene->smoothSelectedStrokes(m_window->m_settings.strokeSmoothing);
        if (smoothed > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Smoothed %1 stroke(s)").arg(smoothed), 3000);
        } else {
            m_window->statusBar()->showMessage(MainWindow::tr("No strokes need smoothing"), 3000);
        }
    });
    m_window->m_selectAllAct = new QAction(MainWindow::tr("Select all items"), m_window);
    m_window->m_selectAllAct->setShortcut(QKeySequence::SelectAll);
    m_window->m_selectAllAct->setStatusTip(MainWindow::tr("Select all items (Ctrl+A)"));
    m_window->m_itemPropsAct =
        makeAction(m_window, IconId::Settings, MainWindow::tr("Item properties…"), QKeySequence(QStringLiteral("Q")),
                   MainWindow::tr("Show the selected item's properties (Q)"));
    m_window->m_rotateCwAct = makeAction(m_window, IconId::RotateCw, MainWindow::tr("Rotate 45° clockwise"),
                                         QKeySequence(QStringLiteral("Ctrl+R")),
                                         MainWindow::tr("Rotate selected items 45° clockwise (Ctrl+R)"));
    m_window->m_rotateCcwAct = makeAction(m_window, IconId::RotateCcw, MainWindow::tr("Rotate 45° counterclockwise"),
                                          QKeySequence(QStringLiteral("Ctrl+Shift+R")),
                                          MainWindow::tr("Rotate selected items 45° counterclockwise (Ctrl+Shift+R)"));

    m_window->m_viewModeAct =
        makeAction(m_window, IconId::View, MainWindow::tr("View mode"), QKeySequence(QStringLiteral("V")),
                   MainWindow::tr("View mode: overview, pan and zoom (V)"));
    m_window->m_brushModeAct =
        makeAction(m_window, IconId::Brush, MainWindow::tr("Brush mode"), QKeySequence(QStringLiteral("B")),
                   MainWindow::tr("Brush mode: hold the left button to draw freely (B)"));
    m_window->m_editModeAct =
        makeAction(m_window, IconId::Edit, MainWindow::tr("Edit mode"), QKeySequence(QStringLiteral("E")),
                   MainWindow::tr("Edit mode: select, move and resize items; hold Ctrl to pan the view (E)"));
    m_window->m_textModeAct =
        makeAction(m_window, IconId::Text, MainWindow::tr("Text mode"), QKeySequence(QStringLiteral("T")),
                   MainWindow::tr("Text mode: click the canvas to place a text item (T)"));
    m_window->m_viewModeAct->setCheckable(true);
    m_window->m_brushModeAct->setCheckable(true);
    m_window->m_editModeAct->setCheckable(true);
    m_window->m_textModeAct->setCheckable(true);
    m_window->m_viewModeAct->setChecked(true);

    m_window->m_modeGroup = new QActionGroup(m_window);
    m_window->m_modeGroup->setExclusive(true);
    m_window->m_modeGroup->addAction(m_window->m_viewModeAct);
    m_window->m_modeGroup->addAction(m_window->m_editModeAct);
    m_window->m_modeGroup->addAction(m_window->m_brushModeAct);
    m_window->m_modeGroup->addAction(m_window->m_textModeAct);
    m_window->m_viewModeAct->setData(static_cast<int>(ToolMode::View));
    m_window->m_brushModeAct->setData(static_cast<int>(ToolMode::Brush));
    m_window->m_editModeAct->setData(static_cast<int>(ToolMode::Edit));
    m_window->m_textModeAct->setData(static_cast<int>(ToolMode::Text));

    m_window->m_fitAct =
        makeAction(m_window, IconId::Fit, MainWindow::tr("Back to overview"), QKeySequence(QStringLiteral("Ctrl+0")),
                   MainWindow::tr("Fit all items (Ctrl+0 / Home)"));
    m_window->m_fitAct->setShortcuts({QKeySequence(QStringLiteral("Ctrl+0")), QKeySequence(Qt::Key_Home)});
    m_window->m_zoomInAct = makeAction(m_window, IconId::ZoomIn, MainWindow::tr("Zoom in"),
                                       QKeySequence(QStringLiteral("Ctrl+=")), MainWindow::tr("Zoom in (Ctrl+=)"));
    m_window->m_zoomOutAct = makeAction(m_window, IconId::ZoomOut, MainWindow::tr("Zoom out"),
                                        QKeySequence(QStringLiteral("Ctrl+-")), MainWindow::tr("Zoom out (Ctrl+-)"));
    m_window->m_actualSizeAct =
        makeAction(m_window, IconId::ZoomIn, MainWindow::tr("Actual size"), QKeySequence(QStringLiteral("Ctrl+1")),
                   MainWindow::tr("Show at 100% (Ctrl+1)"));

    m_window->m_gridAct = makeAction(m_window, IconId::Grid, MainWindow::tr("Helper grid"), QKeySequence(),
                                     MainWindow::tr("Show or hide the helper grid on the canvas"));
    m_window->m_gridAct->setObjectName(QStringLiteral("GridToggleAction"));
    m_window->m_gridAct->setCheckable(true);
    m_window->m_gridAct->setChecked(m_window->m_settings.showGrid);
    QObject::connect(m_window->m_gridAct, &QAction::toggled, m_window, [this](bool visible) {
        m_window->m_scene->setShowGrid(visible);
        m_window->m_settings.showGrid = visible;
        m_window->m_settings.save();
        m_window->statusBar()->showMessage(
            visible ? MainWindow::tr("Helper grid shown") : MainWindow::tr("Helper grid hidden"), 2000);
    });

    m_window->m_replaceBgAct = makeAction(m_window, IconId::Background, MainWindow::tr("Replace background image…"),
                                          QKeySequence(), MainWindow::tr("Choose an image as the canvas background"));
    m_window->m_bgColorAct = makeAction(m_window, IconId::Palette, MainWindow::tr("Set background color…"),
                                        QKeySequence(), MainWindow::tr("Use a solid color as the canvas background"));
    m_window->m_clearBgAct = makeAction(m_window, IconId::Delete, MainWindow::tr("Clear background"), QKeySequence(),
                                        MainWindow::tr("Restore the white background"));

    m_window->m_bgStretchAct = new QAction(MainWindow::tr("Stretch to fill"), m_window);
    m_window->m_bgFitAct = new QAction(MainWindow::tr("Fit entirely"), m_window);
    m_window->m_bgTileAct = new QAction(MainWindow::tr("Tile"), m_window);
    m_window->m_bgStretchAct->setCheckable(true);
    m_window->m_bgFitAct->setCheckable(true);
    m_window->m_bgTileAct->setCheckable(true);
    m_window->m_backgroundModeGroup = new QActionGroup(m_window);
    m_window->m_backgroundModeGroup->setExclusive(true);
    m_window->m_backgroundModeGroup->addAction(m_window->m_bgStretchAct);
    m_window->m_backgroundModeGroup->addAction(m_window->m_bgFitAct);
    m_window->m_backgroundModeGroup->addAction(m_window->m_bgTileAct);

    m_window->m_settingsAct =
        makeAction(m_window, IconId::Settings, MainWindow::tr("Settings…"), QKeySequence(QStringLiteral("Ctrl+,")),
                   MainWindow::tr("Open board settings (Ctrl+,)"));

    m_window->m_aboutAct =
        new QAction(QIcon(QStringLiteral(":/icons/app.png")), MainWindow::tr("About StoryBoard"), m_window);
    m_window->m_shortcutsAct = new QAction(MainWindow::tr("Keyboard shortcuts…"), m_window);
    m_window->m_shortcutsAct->setStatusTip(MainWindow::tr("List every action that has a shortcut"));
    m_window->m_aboutQtAct = new QAction(MainWindow::tr("About Qt"), m_window);

    QObject::connect(m_window->m_newAct, &QAction::triggered, m_window, &MainWindow::newCanvas);
    QObject::connect(m_window->m_openAct, &QAction::triggered, m_window, &MainWindow::openCanvasDialog);
    QObject::connect(m_window->m_saveAct, &QAction::triggered, m_window, [this]() { m_window->saveCanvas(); });
    QObject::connect(m_window->m_saveAsAct, &QAction::triggered, m_window, [this]() { m_window->saveCanvasAs(); });
    QObject::connect(m_window->m_importAct, &QAction::triggered, m_window, &MainWindow::importImagesDialog);
    QObject::connect(m_window->m_lineArtAct, &QAction::triggered, m_window, &MainWindow::importOutlineImageDialog);
    QObject::connect(m_window->m_exportAct, &QAction::triggered, m_window, &MainWindow::exportCanvas);
    QObject::connect(m_window->m_quitAct, &QAction::triggered, m_window, &QWidget::close);

    QObject::connect(m_window->m_undoAct, &QAction::triggered, m_window->m_undo, &QUndoStack::undo);
    QObject::connect(m_window->m_redoAct, &QAction::triggered, m_window->m_undo, &QUndoStack::redo);
    QObject::connect(m_window->m_deleteAct, &QAction::triggered, m_window->m_scene, &CanvasScene::deleteSelected);
    QObject::connect(m_window->m_selectAllAct, &QAction::triggered, m_window->m_scene, &CanvasScene::selectAllItems);
    QObject::connect(m_window->m_itemPropsAct, &QAction::triggered, m_window, &MainWindow::showSelectedItemProperties);
    QObject::connect(m_window->m_rotateCwAct, &QAction::triggered, m_window,
                     [this]() { m_window->rotateSelected(45.0); });
    QObject::connect(m_window->m_rotateCcwAct, &QAction::triggered, m_window,
                     [this]() { m_window->rotateSelected(-45.0); });

    QObject::connect(m_window->m_modeGroup, &QActionGroup::triggered, m_window,
                     [this](QAction* action) { m_window->setToolMode(static_cast<ToolMode>(action->data().toInt())); });
    QObject::connect(m_window->m_fitAct, &QAction::triggered, m_window->m_view, &CanvasView::fitAll);
    QObject::connect(m_window->m_zoomInAct, &QAction::triggered, m_window->m_view, &CanvasView::zoomIn);
    QObject::connect(m_window->m_zoomOutAct, &QAction::triggered, m_window->m_view, &CanvasView::zoomOut);
    QObject::connect(m_window->m_actualSizeAct, &QAction::triggered, m_window->m_view, &CanvasView::actualSize);

    QObject::connect(m_window->m_replaceBgAct, &QAction::triggered, m_window, &MainWindow::replaceBackgroundImage);
    QObject::connect(m_window->m_bgColorAct, &QAction::triggered, m_window, &MainWindow::setBackgroundColor);
    QObject::connect(m_window->m_clearBgAct, &QAction::triggered, m_window, &MainWindow::clearBackground);
    QObject::connect(m_window->m_settingsAct, &QAction::triggered, m_window, &MainWindow::showSettings);

    m_window->m_fillNoneAct = new QAction(MainWindow::tr("No fill"), m_window);
    m_window->m_fillSolidAct = new QAction(MainWindow::tr("Solid fill"), m_window);
    m_window->m_fillHachureAct = new QAction(MainWindow::tr("Hatch fill"), m_window);
    m_window->m_fillReverseAct = new QAction(MainWindow::tr("Reverse hatch fill"), m_window);
    m_window->m_fillCrossAct = new QAction(MainWindow::tr("Cross hatch fill"), m_window);
    m_window->m_fillGridAct = new QAction(MainWindow::tr("Grid hatch fill"), m_window);
    m_window->m_fillHoneycombAct = new QAction(MainWindow::tr("Hexagon fill"), m_window);
    for (QAction* action :
         {m_window->m_fillNoneAct, m_window->m_fillSolidAct, m_window->m_fillHachureAct, m_window->m_fillReverseAct,
          m_window->m_fillCrossAct, m_window->m_fillGridAct, m_window->m_fillHoneycombAct}) {
        action->setCheckable(true);
    }
    m_window->m_fillStyleGroup = new QActionGroup(m_window);
    m_window->m_fillStyleGroup->setExclusive(true);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillNoneAct);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillSolidAct);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillHachureAct);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillReverseAct);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillCrossAct);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillGridAct);
    m_window->m_fillStyleGroup->addAction(m_window->m_fillHoneycombAct);
    m_window->m_fillSolidAct->setChecked(true);
    QObject::connect(m_window->m_fillNoneAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::None); });
    QObject::connect(m_window->m_fillSolidAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::Solid); });
    QObject::connect(m_window->m_fillHachureAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::Hachure); });
    QObject::connect(m_window->m_fillReverseAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::ReverseHachure); });
    QObject::connect(m_window->m_fillCrossAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::CrossHatch); });
    QObject::connect(m_window->m_fillGridAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::GridHatch); });
    QObject::connect(m_window->m_fillHoneycombAct, &QAction::triggered, m_window,
                     [this]() { m_window->setFillStyle(FillStyle::Honeycomb); });
    m_window->m_fillColorAct = new QAction(MainWindow::tr("Fill color…"), m_window);
    m_window->m_fillColorAct->setObjectName(QStringLiteral("FillColorAction"));
    m_window->m_fillColorAct->setIcon(m_window->m_style->fillColorIcon(
        m_window->m_fillColor.isValid() ? m_window->m_fillColor : QColor(30, 41, 59, 36), 16));
    QObject::connect(m_window->m_fillColorAct, &QAction::triggered, m_window, &MainWindow::chooseFillColor);
    m_window->m_strokeColorAct = new QAction(MainWindow::tr("Border color…"), m_window);
    m_window->m_strokeColorAct->setObjectName(QStringLiteral("BorderColorAction"));
    m_window->m_strokeColorAct->setIcon(m_window->m_style->strokeColorIcon(m_window->m_brushColor, 16));
    QObject::connect(m_window->m_strokeColorAct, &QAction::triggered, m_window, &MainWindow::chooseStrokeColor);
    m_window->m_brushColorAct = new QAction(MainWindow::tr("Brush color…"), m_window);
    m_window->m_brushColorAct->setObjectName(QStringLiteral("BrushColorAction"));
    m_window->m_brushColorAct->setIcon(makeColorIcon(m_window->m_brushColor, 16));
    QObject::connect(m_window->m_brushColorAct, &QAction::triggered, m_window, &MainWindow::chooseBrushColor);

    // 需求 31：画笔样式菜单下提供粗细选项，并与工具栏下拉共用同一个设置。
    m_window->m_brushWidthGroup = new QActionGroup(m_window);
    m_window->m_brushWidthGroup->setExclusive(true);
    for (int width : brushWidthPresets()) {
        auto* action = new QAction(QStringLiteral("%1 px").arg(width), m_window);
        action->setObjectName(QStringLiteral("BrushWidthAction%1").arg(width));
        action->setData(width);
        action->setCheckable(true);
        m_window->m_brushWidthGroup->addAction(action);
        QObject::connect(action, &QAction::triggered, m_window, [this, width]() {
            m_window->setBrushWidthUi(width);
            m_window->m_settings.brushWidth = width;
            m_window->m_settings.save();
        });
    }

    auto applyBackgroundMode = [this](BackgroundMode mode) {
        BackgroundState background = m_window->m_scene->background();
        if (background.type != BackgroundType::Image) {
            return;
        }
        background.mode = mode;
        m_window->m_scene->replaceBackground(background, MainWindow::tr("Adjust background fill mode"));
    };
    QObject::connect(m_window->m_bgStretchAct, &QAction::triggered, m_window,
                     [applyBackgroundMode]() { applyBackgroundMode(BackgroundMode::Stretch); });
    QObject::connect(m_window->m_bgFitAct, &QAction::triggered, m_window,
                     [applyBackgroundMode]() { applyBackgroundMode(BackgroundMode::Fit); });
    QObject::connect(m_window->m_bgTileAct, &QAction::triggered, m_window,
                     [applyBackgroundMode]() { applyBackgroundMode(BackgroundMode::Tile); });

    QObject::connect(m_window->m_aboutAct, &QAction::triggered, m_window, &MainWindow::showAbout);
    QObject::connect(m_window->m_shortcutsAct, &QAction::triggered, m_window, &MainWindow::showShortcuts);
    QObject::connect(m_window->m_aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void UiBuilderController::buildShapeMenu()
{
    m_window->m_shapeMenu = new QMenu(MainWindow::tr("Common shapes"), m_window);
    m_window->m_shapeMenu->setObjectName(QStringLiteral("ShapeMenu"));
    m_window->m_shapeMenu->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    m_window->m_shapeMenu->setToolTipsVisible(true);

    for (ShapeType type : allShapeTypes()) {
        QAction* action =
            m_window->m_shapeMenu->addAction(makeShapeIcon(type, QColor(58, 64, 80), 18), shapeTypeName(type));
        action->setObjectName(QStringLiteral("ShapeAction_%1").arg(shapeTypeToString(type)));
        action->setData(static_cast<int>(type));
        action->setToolTip(MainWindow::tr("Insert %1 (uses the current brush color)").arg(shapeTypeName(type)));
        QObject::connect(action, &QAction::triggered, m_window, [this, type]() { m_window->insertShape(type); });
    }

    // 需求：画布格式的自定义图形追加在“常见图形”下方的下拉菜单里，每次展开时重扫目录，
    // 用户新放进 shapes 目录的文件不用重启就能出现。
    m_window->m_shapeMenu->addSeparator();
    m_window->m_customShapeMenu = m_window->m_shapeMenu->addMenu(MainWindow::tr("Custom shapes"));
    m_window->m_customShapeMenu->setObjectName(QStringLiteral("CustomShapeMenu"));
    QObject::connect(m_window->m_shapeMenu, &QMenu::aboutToShow, m_window, &MainWindow::updateCustomShapeMenu);
    m_window->updateCustomShapeMenu();
}

void UiBuilderController::buildMenus()
{
    m_window->m_fileMenu = m_window->menuBar()->addMenu(MainWindow::tr("&File"));
    QMenu* fileMenu = m_window->m_fileMenu;
    fileMenu->addAction(m_window->m_newAct);
    fileMenu->addAction(m_window->m_openAct);
    // 需求：在“打开画布”后面提供图片线稿化入口，把色块轮廓变成笔画图元。
    fileMenu->addAction(m_window->m_lineArtAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_window->m_saveAct);
    fileMenu->addAction(m_window->m_saveAsAct);
    fileMenu->addSeparator();
    // 插入图片只留在“插入”菜单，导入图片线稿化只留在文件菜单，避免同一动作多处出现。
    fileMenu->addAction(m_window->m_exportAct);
    // 需求：原“画板”菜单不再作为父菜单存在，它的设置、最近画布与画布文件夹
    // 直接并入文件菜单，位于“退出”之上；这一段由 updateCanvasesMenu 动态重建。
    QAction* boardSectionSeparator = fileMenu->addSeparator();
    m_window->m_boardSectionSeparator = boardSectionSeparator;
    fileMenu->addAction(m_window->m_quitAct);
    QObject::connect(m_window->m_fileMenu, &QMenu::aboutToShow, m_window, &MainWindow::updateCanvasesMenu);
    m_window->updateCanvasesMenu();

    // 需求：“视图”一级菜单紧跟“文件”，放在“编辑”之前。
    QMenu* viewMenu = m_window->menuBar()->addMenu(MainWindow::tr("&View"));
    viewMenu->setObjectName(QStringLiteral("ViewMenu"));
    viewMenu->addAction(m_window->m_viewModeAct);
    viewMenu->addAction(m_window->m_editModeAct);
    viewMenu->addAction(m_window->m_brushModeAct);
    viewMenu->addAction(m_window->m_textModeAct);
    viewMenu->addSeparator();
    viewMenu->addAction(m_window->m_fitAct);
    viewMenu->addAction(m_window->m_zoomInAct);
    viewMenu->addAction(m_window->m_zoomOutAct);
    viewMenu->addAction(m_window->m_actualSizeAct);
    viewMenu->addSeparator();
    // 需求：辅助网格除设置外还要能从视图菜单直接开关，开关状态与设置里的复选框同步。
    viewMenu->addAction(m_window->m_gridAct);
    QAction* layerDockAction = m_window->m_docks->layerDock()->toggleViewAction();
    layerDockAction->setText(MainWindow::tr("Layer panel"));
    layerDockAction->setStatusTip(MainWindow::tr("Show or hide the layer panel"));
    viewMenu->addAction(layerDockAction);

    QAction* itemDockAction = m_window->m_docks->itemDock()->toggleViewAction();
    itemDockAction->setObjectName(QStringLiteral("ItemDockToggleAction"));
    itemDockAction->setText(MainWindow::tr("Edit item panel"));
    itemDockAction->setStatusTip(
        MainWindow::tr("Show or hide the edit item panel (it appears automatically when an item is selected)"));
    viewMenu->addAction(itemDockAction);

    QMenu* editMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Edit"));
    editMenu->addAction(m_window->m_undoAct);
    editMenu->addAction(m_window->m_redoAct);
    editMenu->addSeparator();
    editMenu->addAction(m_window->m_deleteAct);
    editMenu->addAction(m_window->m_duplicateAct);
    editMenu->addAction(m_window->m_groupAct);
    editMenu->addAction(m_window->m_ungroupAct);
    // 需求：平滑笔画移到“笔画”菜单下，不再留在编辑菜单。
    editMenu->addAction(m_window->m_selectAllAct);
    editMenu->addSeparator();
    editMenu->addAction(m_window->m_itemPropsAct);
    editMenu->addSeparator();

    auto applySelectedHeight = [this](int delta) {
        if (m_window->m_view->mode() != ToolMode::Edit) {
            m_window->statusBar()->showMessage(MainWindow::tr("Height adjustment is available in edit mode only"),
                                               3000);
            return;
        }
        const int changed = m_window->m_scene->adjustSelectedHeights(delta);
        if (changed == 0) {
            m_window->statusBar()->showMessage(
                MainWindow::tr("No selected items can change height (or the height limit was reached)"), 3000);
        } else {
            m_window->statusBar()->showMessage(MainWindow::tr("Changed the height of %1 item(s) by %2")
                                                   .arg(changed)
                                                   .arg(delta > 0 ? QStringLiteral("+1") : QStringLiteral("-1")),
                                               3000);
        }
    };

    // 高度两项只写动作名：快捷键交给 QAction 自己显示，手写 “] / [” 会跟它重复一遍。
    QAction* heightUpAct = editMenu->addAction(MainWindow::tr("Height +1 (stack up)"));
    heightUpAct->setObjectName(QStringLiteral("HeightUpAction"));
    heightUpAct->setIcon(makeIcon(IconId::OrderUp, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    heightUpAct->setShortcut(QKeySequence(Qt::Key_BracketRight));
    heightUpAct->setStatusTip(MainWindow::tr("Raise the stacking height of selected items (shortcut ])"));
    QObject::connect(heightUpAct, &QAction::triggered, m_window, [applySelectedHeight]() { applySelectedHeight(1); });

    QAction* heightDownAct = editMenu->addAction(MainWindow::tr("Height -1 (stack down)"));
    heightDownAct->setObjectName(QStringLiteral("HeightDownAction"));
    heightDownAct->setIcon(makeIcon(IconId::OrderDown, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    heightDownAct->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    heightDownAct->setStatusTip(MainWindow::tr("Lower the stacking height of selected items (shortcut [)"));
    QObject::connect(heightDownAct, &QAction::triggered, m_window,
                     [applySelectedHeight]() { applySelectedHeight(-1); });

    editMenu->addSeparator();
    editMenu->addAction(m_window->m_rotateCwAct);
    editMenu->addAction(m_window->m_rotateCcwAct);

    // 多图元对齐：编辑菜单里做成下拉子菜单，工具栏上再给一个下拉按钮，共用同一组动作。
    QMenu* alignMenu = editMenu->addMenu(MainWindow::tr("Align"));
    alignMenu->setObjectName(QStringLiteral("AlignMenu"));
    struct AlignEntry {
        Alignment::Mode mode;
        const char* text;
        const char* tip;
    };
    const AlignEntry alignEntries[] = {
        {Alignment::Mode::Left, QT_TR_NOOP("Align left"), QT_TR_NOOP("Line up the left edges of the selected items")},
        {Alignment::Mode::HorizontalCenter, QT_TR_NOOP("Align horizontal centers"),
         QT_TR_NOOP("Center the selected items on the same vertical axis")},
        {Alignment::Mode::Right, QT_TR_NOOP("Align right"),
         QT_TR_NOOP("Line up the right edges of the selected items")},
        {Alignment::Mode::Top, QT_TR_NOOP("Align top"), QT_TR_NOOP("Line up the top edges of the selected items")},
        {Alignment::Mode::VerticalCenter, QT_TR_NOOP("Align vertical centers"),
         QT_TR_NOOP("Center the selected items on the same horizontal axis")},
        {Alignment::Mode::Bottom, QT_TR_NOOP("Align bottom"),
         QT_TR_NOOP("Line up the bottom edges of the selected items")},
        {Alignment::Mode::DistributeHorizontal, QT_TR_NOOP("Distribute horizontally"),
         QT_TR_NOOP("Space the selected items evenly from left to right (needs 3 or more)")},
        {Alignment::Mode::DistributeVertical, QT_TR_NOOP("Distribute vertically"),
         QT_TR_NOOP("Space the selected items evenly from top to bottom (needs 3 or more)")},
    };
    for (const AlignEntry& entry : alignEntries) {
        QAction* action = alignMenu->addAction(MainWindow::tr(entry.text));
        action->setStatusTip(MainWindow::tr(entry.tip));
        action->setProperty("alignMode", static_cast<int>(entry.mode));
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, mode = entry.mode]() { m_window->applyAlignment(mode); });
        m_window->m_alignActions.append(action);
    }

    // 合并形状：与 PPT「合并形状」同一套命名——联合 / 组合 / 拆分 / 相交 / 剪除。
    QMenu* boolMenu = editMenu->addMenu(MainWindow::tr("Merge shapes"));
    boolMenu->setObjectName(QStringLiteral("BoolMenu"));
    boolMenu->setIcon(makeIcon(IconId::MergeUnion, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    const QList<QPair<BoolOps::Operation, QString>> boolEntries = {
        {BoolOps::Operation::Union, MainWindow::tr("Union")},
        {BoolOps::Operation::Combine, MainWindow::tr("Combine")},
        {BoolOps::Operation::Fragment, MainWindow::tr("Fragment")},
        {BoolOps::Operation::Intersect, MainWindow::tr("Intersect")},
        {BoolOps::Operation::Subtract, MainWindow::tr("Subtract")},
    };
    const QList<QString> boolTips = {
        MainWindow::tr("Merge the selected items into one merged shape"),
        MainWindow::tr("Merge the outlines and cut out the overlapping part"),
        MainWindow::tr("Split the selected items into separate pieces at every overlap"),
        MainWindow::tr("Keep only the overlapping part"),
        MainWindow::tr("Cut the lower items out of the topmost item"),
    };
    // 图标与 PPT「合并形状」一致：两个交叠圆表示各运算的结果区域。
    const QList<IconId> boolIcons = {IconId::MergeUnion, IconId::MergeCombine, IconId::MergeFragment,
                                     IconId::MergeIntersect, IconId::MergeSubtract};
    for (int i = 0; i < boolEntries.size(); ++i) {
        QAction* action = boolMenu->addAction(boolEntries.at(i).second);
        action->setIcon(
            makeIcon(boolIcons.at(qMin(i, boolIcons.size() - 1)), QColor(58, 64, 80), MainWindow::kToolBarIconSize));
        action->setStatusTip(boolTips.at(qMin(i, boolTips.size() - 1)));
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, operation = boolEntries.at(i).first]() { m_window->applyBoolOperation(operation); });
        m_window->m_boolActions.append(action);
    }
    // 多选时编辑图元卡直接复用同一批对齐 / 合并形状动作，快捷键与启用状态不用再维护一份。
    if (m_window->m_docks && m_window->m_docks->itemPanel()) {
        m_window->m_docks->itemPanel()->setSelectionActions(m_window->m_alignActions, m_window->m_boolActions);
        m_window->m_docks->itemPanel()->refresh();
    }

    // 需求：原“样式”菜单拆开——“画笔样式”独立为一级菜单“画笔”，
    // 填充样式与填充／描边颜色并入“插入”菜单，紧跟在“常见图形”之后。
    QMenu* insertMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Insert"));
    insertMenu->setObjectName(QStringLiteral("InsertMenu"));

    insertMenu->addMenu(m_window->m_shapeMenu);
    insertMenu->addSeparator();

    QMenu* fillStyleMenu = insertMenu->addMenu(MainWindow::tr("Fill style"));
    fillStyleMenu->setObjectName(QStringLiteral("FillStyleMenu"));
    fillStyleMenu->setIcon(m_window->m_style->fillStyleIcon(
        m_window->m_fillStyle, m_window->m_fillColor.isValid() ? m_window->m_fillColor : m_window->m_brushColor, 16));
    fillStyleMenu->addAction(m_window->m_fillNoneAct);
    fillStyleMenu->addAction(m_window->m_fillSolidAct);
    fillStyleMenu->addAction(m_window->m_fillHachureAct);
    fillStyleMenu->addAction(m_window->m_fillReverseAct);
    fillStyleMenu->addAction(m_window->m_fillCrossAct);
    fillStyleMenu->addAction(m_window->m_fillGridAct);
    fillStyleMenu->addAction(m_window->m_fillHoneycombAct);

    // 需求 29：颜色从填充样式里分离出来，放在填充样式之后。
    insertMenu->addAction(m_window->m_fillColorAct);
    insertMenu->addAction(m_window->m_strokeColorAct);
    // 需求：原“画布”菜单不再作为父菜单存在，背景相关项直接并入“插入”菜单；
    // 背景属于画布属性，排在“插入图片”之前，避免和插入类动作混在一段。
    insertMenu->addSeparator();
    insertMenu->addAction(m_window->m_replaceBgAct);
    insertMenu->addAction(m_window->m_bgColorAct);
    insertMenu->addAction(m_window->m_clearBgAct);
    QMenu* fillMenu = insertMenu->addMenu(MainWindow::tr("Background fill mode"));
    fillMenu->setIcon(makeIcon(IconId::Background, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    fillMenu->addAction(m_window->m_bgStretchAct);
    fillMenu->addAction(m_window->m_bgFitAct);
    fillMenu->addAction(m_window->m_bgTileAct);

    insertMenu->addSeparator();
    insertMenu->addAction(m_window->m_importAct);

    // 需求：原“样式 → 画笔样式”提升为一级菜单“画笔”，“平滑笔画”归到这里。
    QMenu* brushMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Brush"));
    brushMenu->setObjectName(QStringLiteral("BrushMenu"));

    brushMenu->addAction(m_window->m_brushColorAct);
    brushMenu->addSeparator();
    QMenu* brushWidthMenu = brushMenu->addMenu(MainWindow::tr("Brush width"));
    brushWidthMenu->setObjectName(QStringLiteral("BrushWidthMenu"));
    for (QAction* action : m_window->m_brushWidthGroup->actions()) {
        brushWidthMenu->addAction(action);
    }
    brushMenu->addSeparator();
    brushMenu->addAction(m_window->m_smoothAct);

    QMenu* helpMenu = m_window->menuBar()->addMenu(MainWindow::tr("&Help"));
    helpMenu->addAction(m_window->m_shortcutsAct);
    helpMenu->addAction(m_window->m_aboutAct);
    helpMenu->addAction(m_window->m_aboutQtAct);
}

void UiBuilderController::buildToolBars()
{
    auto* mainBar = new QToolBar(MainWindow::tr("Main toolbar"), m_window);
    mainBar->setObjectName(QStringLiteral("MainToolBar"));
    mainBar->setMovable(false);
    mainBar->setFloatable(false);
    mainBar->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    mainBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    mainBar->setContextMenuPolicy(Qt::PreventContextMenu);
    m_window->addToolBar(Qt::TopToolBarArea, mainBar);

    mainBar->addAction(m_window->m_newAct);
    mainBar->addAction(m_window->m_openAct);
    // 需求：线稿化入口放在“保存画布”按钮前面，和文件菜单里的位置对应。
    mainBar->addAction(m_window->m_lineArtAct);
    mainBar->addAction(m_window->m_saveAct);
    mainBar->addAction(m_window->m_exportAct);
    mainBar->addSeparator();
    mainBar->addAction(m_window->m_undoAct);
    mainBar->addAction(m_window->m_redoAct);
    // 需求 40：顺/逆时针旋转 45° 除了菜单快捷键，也必须出现在工具栏。
    mainBar->addAction(m_window->m_rotateCwAct);
    mainBar->addAction(m_window->m_rotateCcwAct);

    // 对齐下拉按钮：和编辑菜单里的对齐子菜单共用同一组动作，展开就是一个下拉清单。
    m_window->m_alignButton = new QToolButton(mainBar);
    m_window->m_alignButton->setObjectName(QStringLiteral("AlignToolButton"));
    m_window->m_alignButton->setText(MainWindow::tr("Align"));
    m_window->m_alignButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_window->m_alignButton->setPopupMode(QToolButton::InstantPopup);
    m_window->m_alignButton->setToolTip(MainWindow::tr("Align or distribute the selected items"));
    auto* alignButtonMenu = new QMenu(m_window->m_alignButton);
    alignButtonMenu->setObjectName(QStringLiteral("AlignToolButtonMenu"));
    for (QAction* action : std::as_const(m_window->m_alignActions)) {
        alignButtonMenu->addAction(action);
    }
    m_window->m_alignButton->setMenu(alignButtonMenu);
    mainBar->addWidget(m_window->m_alignButton);

    // 布尔运算同样做成工具栏下拉按钮，菜单里再挂一份到画布右键菜单。
    m_window->m_boolButton = new QToolButton(mainBar);
    m_window->m_boolButton->setObjectName(QStringLiteral("BoolToolButton"));
    m_window->m_boolButton->setText(MainWindow::tr("Merge shapes"));
    m_window->m_boolButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_window->m_boolButton->setPopupMode(QToolButton::InstantPopup);
    m_window->m_boolButton->setToolTip(MainWindow::tr("Merge the selected shapes"));
    m_window->m_boolButton->setEnabled(false);
    auto* boolButtonMenu = new QMenu(m_window->m_boolButton);
    boolButtonMenu->setObjectName(QStringLiteral("BoolToolButtonMenu"));
    for (QAction* action : std::as_const(m_window->m_boolActions)) {
        boolButtonMenu->addAction(action);
    }
    m_window->m_boolButton->setMenu(boolButtonMenu);
    mainBar->addWidget(m_window->m_boolButton);
    if (m_window->m_view) {
        m_window->m_view->addContextMenuGroup(MainWindow::tr("Merge shapes"), m_window->m_boolActions);
    }
    mainBar->addSeparator();

    createFillToolbar(mainBar);
    mainBar->addSeparator();
    createBrushToolbar(mainBar);
    mainBar->addSeparator();

    mainBar->addAction(m_window->m_fitAct);
    mainBar->addAction(m_window->m_zoomInAct);
    mainBar->addAction(m_window->m_zoomOutAct);
    mainBar->addSeparator();
    mainBar->addAction(m_window->m_importAct);
    mainBar->addAction(m_window->m_replaceBgAct);
    mainBar->addAction(m_window->m_settingsAct);

    // 统一主工具栏里所有纯图标按钮的尺寸：QToolBar 为 action 自动创建的按钮、
    // 以及 addWidget 加入的色板/填充按钮，都钉成同一个正方形，避免按各自
    // sizeHint 算出宽窄不一的方块。带文字或下拉菜单的按钮保持自身宽度。
    for (QToolButton* button : mainBar->findChildren<QToolButton*>()) {
        if (button->toolButtonStyle() != Qt::ToolButtonIconOnly) {
            continue;
        }
        if (button == m_window->m_colorButton) {
            button->setFixedSize(MainWindow::kToolBarSplitButtonWidth, MainWindow::kToolBarButtonSize);
            continue;
        }
        button->setFixedSize(MainWindow::kToolBarButtonSize, MainWindow::kToolBarButtonSize);
    }

    auto* modeBar = new QToolBar(MainWindow::tr("Mode"), m_window);
    modeBar->setObjectName(QStringLiteral("ModeToolBar"));
    modeBar->setMovable(false);
    modeBar->setFloatable(false);
    modeBar->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    modeBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modeBar->setContextMenuPolicy(Qt::PreventContextMenu);
    modeBar->setMinimumWidth(80);
    m_window->addToolBar(Qt::LeftToolBarArea, modeBar);
    modeBar->addAction(m_window->m_viewModeAct);
    modeBar->addAction(m_window->m_editModeAct);
    modeBar->addAction(m_window->m_brushModeAct);
    modeBar->addAction(m_window->m_textModeAct);
}

void UiBuilderController::createBrushToolbar(QToolBar* mainBar)
{
    auto* brushLabel = new QLabel(MainWindow::tr("Brush"), mainBar);
    brushLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(brushLabel);
    m_window->m_colorButton = new QToolButton(mainBar);
    m_window->m_colorButton->setObjectName(QStringLiteral("ColorButton"));
    m_window->m_colorButton->setIcon(makeColorIcon(m_window->m_brushColor, MainWindow::kToolBarIconSize));
    m_window->m_colorButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_colorButton->setToolTip(MainWindow::tr("Brush color: %1 (click to choose)")
                                            .arg(m_window->m_brushColor.name(QColor::HexArgb).toUpper()));
    m_window->m_colorButton->setPopupMode(QToolButton::MenuButtonPopup);

    auto* colorMenu = new QMenu(m_window->m_colorButton);
    const QList<QPair<QString, QColor>> presets = {{MainWindow::tr("Ink black"), QColor(QStringLiteral("#111827"))},
                                                   {MainWindow::tr("Red"), QColor(QStringLiteral("#EF4444"))},
                                                   {MainWindow::tr("Orange"), QColor(QStringLiteral("#F59E0B"))},
                                                   {MainWindow::tr("Green"), QColor(QStringLiteral("#10B981"))},
                                                   {MainWindow::tr("Blue"), QColor(QStringLiteral("#3B82F6"))},
                                                   {MainWindow::tr("Purple"), QColor(QStringLiteral("#8B5CF6"))},
                                                   {MainWindow::tr("Pink"), QColor(QStringLiteral("#EC4899"))},
                                                   {MainWindow::tr("White"), QColor(QStringLiteral("#FFFFFF"))}};
    for (const auto& preset : presets) {
        QAction* action = colorMenu->addAction(makeColorIcon(preset.second, 16), preset.first);
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, color = preset.second]() { m_window->setBrushColor(color, true); });
    }
    colorMenu->addSeparator();
    QAction* customColor = colorMenu->addAction(MainWindow::tr("Custom color…"));
    QObject::connect(customColor, &QAction::triggered, m_window, &MainWindow::chooseBrushColor);
    m_window->m_colorButton->setMenu(colorMenu);
    QObject::connect(m_window->m_colorButton, &QToolButton::clicked, m_window, &MainWindow::chooseBrushColor);
    mainBar->addWidget(m_window->m_colorButton);

    m_window->m_widthCombo = new QComboBox(mainBar);
    m_window->m_widthCombo->setObjectName(QStringLiteral("BrushWidthCombo"));
    m_window->m_widthCombo->setFixedWidth(74);
    m_window->m_widthCombo->setFixedHeight(MainWindow::kToolBarControlHeight);
    m_window->m_widthCombo->setToolTip(MainWindow::tr("Brush width"));
    for (int width : brushWidthPresets()) {
        m_window->m_widthCombo->addItem(QStringLiteral("%1 px").arg(width), width);
    }
    QObject::connect(m_window->m_widthCombo, &QComboBox::currentIndexChanged, m_window, [this](int index) {
        const int value = m_window->m_widthCombo->itemData(index).toInt();
        if (value <= 0) {
            return;
        }
        m_window->m_brushWidth = value;
        m_window->m_view->setBrushWidth(value);
        m_window->m_settings.brushWidth = value;
        m_window->m_settings.save();
        if (m_window->m_brushWidthGroup) {
            for (QAction* action : m_window->m_brushWidthGroup->actions()) {
                if (action->data().toInt() == value) {
                    const QSignalBlocker blocker(action);
                    action->setChecked(true);
                }
            }
        }
    });
    m_window->setBrushWidthUi(m_window->m_brushWidth);
    auto* widthLabel = new QLabel(MainWindow::tr("Width"), mainBar);
    widthLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(widthLabel);
    mainBar->addWidget(m_window->m_widthCombo);

    auto* smoothButton = new QToolButton(mainBar);
    smoothButton->setObjectName(QStringLiteral("SmoothStrokeButton"));
    smoothButton->setDefaultAction(m_window->m_smoothAct);
    // 这个按钮由工具栏 addWidget 加入，不会自动继承 QToolBar::iconSize，
    // 必须显式设置，否则图标会按样式默认尺寸绘制、比旁边按钮大一圈。
    smoothButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    smoothButton->setFixedHeight(MainWindow::kToolBarButtonSize);
    // 只显示图标，与相邻按钮等宽；功能靠悬停提示说明。
    smoothButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    smoothButton->setToolTip(MainWindow::tr("Smooth strokes once"));
    mainBar->addWidget(smoothButton);

    auto* smoothingLabel = new QLabel(MainWindow::tr("Tolerance"), mainBar);
    // 紧跟图标按钮的标签，留白收到最小，避免和按钮内边距叠成一条缝。
    smoothingLabel->setObjectName(QStringLiteral("ToolBarLabelTight"));
    mainBar->addWidget(smoothingLabel);

    m_window->m_smoothingSpin = new QSpinBox(mainBar);
    m_window->m_smoothingSpin->setObjectName(QStringLiteral("BrushSmoothingSpin"));
    m_window->m_smoothingSpin->setRange(0, 30);
    m_window->m_smoothingSpin->setSuffix(QStringLiteral(" px"));
    m_window->m_smoothingSpin->setFixedWidth(64);
    m_window->m_smoothingSpin->setFixedHeight(MainWindow::kToolBarControlHeight);
    m_window->m_smoothingSpin->setValue(m_window->m_settings.strokeSmoothing);
    m_window->m_smoothingSpin->setToolTip(
        MainWindow::tr("Auto-smoothing tolerance used when a stroke ends; 0 disables automatic smoothing"));
    QObject::connect(m_window->m_smoothingSpin, &QSpinBox::valueChanged, m_window, [this](int value) {
        m_window->m_settings.strokeSmoothing = value;
        m_window->m_view->setStrokeSmoothing(value);
        m_window->m_settings.save();
    });
    mainBar->addWidget(m_window->m_smoothingSpin);
}

void UiBuilderController::createFillToolbar(QToolBar* mainBar)
{
    // 需求 41：填充样式按钮前显示“图形”标签，与画笔区形成明确分组。
    auto* fillLabel = new QLabel(MainWindow::tr("Shape"), mainBar);
    fillLabel->setObjectName(QStringLiteral("ToolBarLabel"));
    mainBar->addWidget(fillLabel);
    auto* shapeButton = new QToolButton(mainBar);
    shapeButton->setObjectName(QStringLiteral("ShapeButton"));
    shapeButton->setIcon(makeIcon(IconId::Shapes, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
    shapeButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    shapeButton->setPopupMode(QToolButton::InstantPopup);
    shapeButton->setMenu(m_window->m_shapeMenu);
    shapeButton->setToolTip(MainWindow::tr("Quickly insert circles, rectangles, arrows, braces and more"));
    shapeButton->setStatusTip(MainWindow::tr("Quickly insert common shapes (uses the current brush color)"));
    mainBar->addWidget(shapeButton);

    m_window->m_fillButton = new QToolButton(mainBar);
    m_window->m_fillButton->setObjectName(QStringLiteral("FillButton"));
    m_window->m_fillButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_fillButton->setPopupMode(QToolButton::InstantPopup);
    auto* fillMenu = new QMenu(m_window->m_fillButton);
    fillMenu->addAction(m_window->m_fillNoneAct);
    fillMenu->addAction(m_window->m_fillSolidAct);
    fillMenu->addAction(m_window->m_fillHachureAct);
    fillMenu->addAction(m_window->m_fillReverseAct);
    fillMenu->addAction(m_window->m_fillCrossAct);
    fillMenu->addAction(m_window->m_fillGridAct);
    fillMenu->addAction(m_window->m_fillHoneycombAct);
    m_window->m_fillButton->setMenu(fillMenu);
    m_window->updateFillActions();
    mainBar->addWidget(m_window->m_fillButton);

    // 需求 29：填充颜色、描边颜色作为独立按钮，排在填充样式按钮之后。
    m_window->m_fillColorButton = new QToolButton(mainBar);
    m_window->m_fillColorButton->setObjectName(QStringLiteral("FillColorButton"));
    m_window->m_fillColorButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_fillColorButton->setDefaultAction(m_window->m_fillColorAct);
    m_window->m_fillColorButton->setToolTip(MainWindow::tr("Fill color: %1 (click to choose)")
                                                .arg(m_window->m_fillColor.isValid()
                                                         ? m_window->m_fillColor.name(QColor::HexArgb).toUpper()
                                                         : MainWindow::tr("Follow brush color")));
    mainBar->addWidget(m_window->m_fillColorButton);

    m_window->m_strokeColorButton = new QToolButton(mainBar);
    m_window->m_strokeColorButton->setObjectName(QStringLiteral("StrokeColorButton"));
    m_window->m_strokeColorButton->setIconSize(QSize(MainWindow::kToolBarIconSize, MainWindow::kToolBarIconSize));
    m_window->m_strokeColorButton->setDefaultAction(m_window->m_strokeColorAct);
    // 菜单项图标是 16px，工具栏按钮另画一份 20px 的，避免比旁边的图标小一圈。
    m_window->m_strokeColorButton->setIcon(
        m_window->m_style->strokeColorIcon(m_window->m_brushColor, MainWindow::kToolBarIconSize));
    m_window->m_strokeColorButton->setToolTip(MainWindow::tr("Border color: %1 (click to choose)")
                                                  .arg(m_window->m_brushColor.name(QColor::HexArgb).toUpper()));
    mainBar->addWidget(m_window->m_strokeColorButton);
}

void UiBuilderController::buildStatusBar()
{
    m_window->statusBar()->setSizeGripEnabled(true);
    m_window->m_hintLabel = new QLabel(m_window);
    m_window->m_hintLabel->setObjectName(QStringLiteral("StatusHint"));
    m_window->m_modeBadge = makeBadge(QStringLiteral("Badge"), m_window);
    m_window->m_zoomBadge = makeBadge(QStringLiteral("Badge"), m_window);
    m_window->m_countBadge = makeBadge(QStringLiteral("Badge"), m_window);

    m_window->statusBar()->addWidget(m_window->m_hintLabel, 1);
    m_window->statusBar()->addPermanentWidget(m_window->m_modeBadge);
    m_window->statusBar()->addPermanentWidget(m_window->m_zoomBadge);
    m_window->statusBar()->addPermanentWidget(m_window->m_countBadge);
}
