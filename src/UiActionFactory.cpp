#include "UiActionFactory.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "StyleController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QStatusBar>
#include <QUndoStack>
#include <QWidget>

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

QList<int> brushWidthPresets()
{
    return {1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
}

} // namespace

UiActionFactory::UiActionFactory(MainWindow* window) : m_window(window) {}

void UiActionFactory::build()
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
    m_window->m_duplicateAct->setIcon(makeIcon(IconId::Copy, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
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
    m_window->m_groupAct->setIcon(makeIcon(IconId::Group, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
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
            // 组合前后“取消组合”的可用状态会变，立即刷新一批动作与卡片。
            m_window->updateActions();
        }
    });
    m_window->m_ungroupAct = new QAction(MainWindow::tr("Ungroup selected items"), m_window);
    m_window->m_ungroupAct->setIcon(makeIcon(IconId::Ungroup, QColor(58, 64, 80), MainWindow::kToolBarIconSize));
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
            m_window->updateActions();
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
                                        MainWindow::tr("Restore a transparent background"));

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
    m_window->m_brushColorAct->setIcon(m_window->m_style->strokeColorIcon(m_window->m_brushColor, 16));
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
