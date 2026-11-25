#include "ui/builder/UiActionFactory.h"

#include "canvas/CanvasView.h"
#include "items/properties/FillStyleMeta.h"
#include "items/properties/ItemStyleVisuals.h"
#include "scene/CanvasScene.h"
#include "ui/icons/IconFactory.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/mainwindow/MainWindowEditActionsController.h"
#include "ui/widgets/StyleController.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QRegularExpression>
#include <QStatusBar>
#include <QUndoStack>
#include <QWidget>

QString StripShortcutTail(const QString& tip)
{
    static const QRegularExpression TAIL(QStringLiteral("(?:\\s*[（(][^（）()]*[）)])$"));
    QString result = tip;
    result.remove(TAIL);
    return result;
}

namespace {

QAction* MakeAction(QObject* parent, IconId icon, const QString& text, const QKeySequence& shortcut = QKeySequence(),
                    QString tip = QString())
{
    // 工具栏按钮与菜单项共用一份图标，按工具栏图标尺寸生成；菜单再缩放到 16px。
    auto* action =
        new QAction(IconFactory::Make(icon, QColor(58, 64, 80), MainWindow::TOOL_BAR_ICON_SIZE), text, parent);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    // 提示里统一带上快捷键，省得逐个动作手写。基串是「不带键位的纯描述」：
    // 作者手写的 “(Ctrl+N)” 在入库前剥掉，改键后 refreshShortcutTips 重拼
    // 只会拼一次，不会再出现双份括号。
    tip = StripShortcutTail(tip);
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

} // namespace

UiActionFactory::UiActionFactory(MainWindow* window) : m_window(window) {}

void UiActionFactory::Build()
{
    m_window->m_newAct =
        MakeAction(m_window, IconId::NEW, MainWindow::tr("New canvas"), QKeySequence(QStringLiteral("Ctrl+N")),
                   MainWindow::tr("Create an empty canvas (Ctrl+N)"));
    m_window->m_openAct =
        MakeAction(m_window, IconId::OPEN, MainWindow::tr("Open canvas…"), QKeySequence(QStringLiteral("Ctrl+O")),
                   MainWindow::tr("Open a StoryBoard canvas (Ctrl+O)"));
    m_window->m_saveAct =
        MakeAction(m_window, IconId::SAVE, MainWindow::tr("Save canvas"), QKeySequence(QStringLiteral("Ctrl+S")),
                   MainWindow::tr("Save the current canvas (Ctrl+S)"));
    m_window->m_saveAsAct = MakeAction(m_window, IconId::SAVE, MainWindow::tr("Save canvas as…"),
                                       QKeySequence(QStringLiteral("Ctrl+Shift+S")),
                                       MainWindow::tr("Save the current canvas to a new file (Ctrl+Shift+S)"));
    // 只留一个主键：Ctrl+I 取 I（image / insert）的直觉，Ctrl+P 留给打印这类通用标准键。
    m_window->m_importAct =
        MakeAction(m_window, IconId::IMAGE, MainWindow::tr("Insert images…"), QKeySequence(QStringLiteral("Ctrl+I")),
                   MainWindow::tr("Insert images as movable, resizable items (Ctrl+I)"));
    m_window->m_lineArtAct = MakeAction(
        m_window, IconId::LINE_ART, MainWindow::tr("Import outline image"), QKeySequence(QStringLiteral("Ctrl+L")),
        MainWindow::tr(
            "Trace an image's color-block outlines into strokes (line width 1); a parameter dialog opens first and "
            "only Save to configuration file writes StoryBoard.ini (Ctrl+L)"));
    m_window->m_exportAct =
        MakeAction(m_window, IconId::EXPORT, MainWindow::tr("Export canvas…"), QKeySequence(QStringLiteral("Ctrl+E")),
                   MainWindow::tr("Export as PNG / JPEG / PDF (Ctrl+E)"));
    m_window->m_exportExcalidrawAct =
        MakeAction(m_window, IconId::EXPORT, MainWindow::tr("Export Excalidraw…"), QKeySequence(),
                   MainWindow::tr("Save this canvas as an .excalidraw scene that excalidraw.com can open"));
    m_window->m_exportCustomShapeAct = MakeAction(
        m_window, IconId::SHAPES, MainWindow::tr("Export custom shape…"), QKeySequence(QStringLiteral("Ctrl+Shift+E")),
        MainWindow::tr("Save the selected items into the shapes folder as a reusable template "
                       "(Ctrl+Shift+E, also works in full screen)"));
    // 主键取 Windows 关闭标准 Alt+F4；副键 Ctrl+Q 是 Linux / macOS 级的退出标准。
    m_window->m_quitAct = new QAction(MainWindow::tr("Quit"), m_window);
    m_window->m_quitAct->setShortcuts({QKeySequence(Qt::ALT | Qt::Key_F4), QKeySequence(QStringLiteral("Ctrl+Q"))});
    m_window->m_quitAct->setStatusTip(MainWindow::tr("Quit StoryBoard (Alt+F4)"));

    m_window->m_undoAct = MakeAction(m_window, IconId::UNDO, MainWindow::tr("Undo"), QKeySequence::Undo,
                                     MainWindow::tr("Undo the last step (Ctrl+Z)"));
    m_window->m_undoAct->setEnabled(false);
    m_window->m_redoAct = MakeAction(m_window, IconId::REDO, MainWindow::tr("Redo"),
                                     QKeySequence(QStringLiteral("Ctrl+Y")), MainWindow::tr("Redo (Ctrl+Y)"));
    // 副键 Ctrl+Shift+Z 是 macOS / GNOME 级的重做标准。
    m_window->m_redoAct->setShortcuts(
        {QKeySequence(QStringLiteral("Ctrl+Y")), QKeySequence(QStringLiteral("Ctrl+Shift+Z"))});
    m_window->m_redoAct->setEnabled(false);

    m_window->m_deleteAct = MakeAction(m_window, IconId::DELETE, MainWindow::tr("Delete selected items"),
                                       QKeySequence::Delete, MainWindow::tr("Delete the selected items (Delete)"));
    m_window->m_duplicateAct = new QAction(MainWindow::tr("Duplicate selected items"), m_window);
    m_window->m_duplicateAct->setIcon(
        IconFactory::Make(IconId::COPY, QColor(58, 64, 80), MainWindow::TOOL_BAR_ICON_SIZE));
    m_window->m_duplicateAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    m_window->m_duplicateAct->setStatusTip(MainWindow::tr("Duplicate the selected items with a small offset (Ctrl+D)"));
    m_window->m_duplicateAct->setEnabled(false);
    QObject::connect(m_window->m_duplicateAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene) {
            return;
        }
        const int duplicated = m_window->m_scene->DuplicateSelectedItems();
        if (duplicated > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Duplicated %n Item(s)", nullptr, duplicated), 3000);
        }
    });
    m_window->m_copyAct = MakeAction(m_window, IconId::COPY, MainWindow::tr("Copy selected items"),
                                     QKeySequence(QStringLiteral("Ctrl+C")),
                                     MainWindow::tr("Copy the selected items to the clipboard (Ctrl+C)"));
    m_window->m_copyAct->setObjectName(QStringLiteral("CopyItemsAction"));
    m_window->m_copyAct->setEnabled(false);
    m_window->m_cutAct = MakeAction(m_window, IconId::DELETE, MainWindow::tr("Cut selected items"),
                                    QKeySequence(QStringLiteral("Ctrl+X")),
                                    MainWindow::tr("Cut the selected items to the clipboard (Ctrl+X)"));
    m_window->m_cutAct->setObjectName(QStringLiteral("CutItemsAction"));
    m_window->m_cutAct->setEnabled(false);
    m_window->m_pasteAct = new QAction(MainWindow::tr("Paste"), m_window);
    m_window->m_pasteAct->setObjectName(QStringLiteral("PasteItemsAction"));
    m_window->m_pasteAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+V")));
    m_window->m_pasteAct->setStatusTip(MainWindow::tr("Paste items from the clipboard (Ctrl+V)"));
    m_window->m_pasteAct->setEnabled(false);

    m_window->m_groupAct = new QAction(MainWindow::tr("Group selected items"), m_window);
    m_window->m_groupAct->setIcon(IconFactory::Make(IconId::GROUP, QColor(58, 64, 80), MainWindow::TOOL_BAR_ICON_SIZE));
    m_window->m_groupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    m_window->m_groupAct->setStatusTip(MainWindow::tr("Group the selected items so they move together (Ctrl+G)"));
    m_window->m_groupAct->setEnabled(false);
    QObject::connect(m_window->m_groupAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene) {
            return;
        }
        const int grouped = m_window->m_scene->GroupSelectedItems();
        if (grouped > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Grouped %n Item(s)", nullptr, grouped), 3000);
            // 组合前后“取消组合”的可用状态会变，立即刷新一批动作与卡片。
            m_window->UpdateActions();
        }
    });
    m_window->m_ungroupAct = new QAction(MainWindow::tr("Ungroup selected items"), m_window);
    m_window->m_ungroupAct->setIcon(
        IconFactory::Make(IconId::UNGROUP, QColor(58, 64, 80), MainWindow::TOOL_BAR_ICON_SIZE));
    m_window->m_ungroupAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    m_window->m_ungroupAct->setStatusTip(MainWindow::tr("Ungroup the selected Group(s) (Ctrl+Shift+G)"));
    m_window->m_ungroupAct->setEnabled(false);
    QObject::connect(m_window->m_ungroupAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene) {
            return;
        }
        const int ungrouped = m_window->m_scene->UngroupSelectedItems();
        if (ungrouped > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Ungrouped %n Item(s)", nullptr, ungrouped), 3000);
            m_window->UpdateActions();
        }
    });
    m_window->m_smoothAct = MakeAction(
        m_window, IconId::SMOOTH, MainWindow::tr("Simplify curve"), QKeySequence(QStringLiteral("Ctrl+Shift+B")),
        MainWindow::tr("Simplify the selected curve once using the current brush tolerance "
                       "(Ctrl+Shift+B)"));
    m_window->m_smoothAct->setEnabled(false);
    QObject::connect(m_window->m_smoothAct, &QAction::triggered, m_window, [this]() {
        if (!m_window->m_scene || !m_window->m_view) {
            return;
        }
        const int smoothed = m_window->m_scene->SmoothSelectedStrokes(m_window->m_settings.strokeSmoothing);
        if (smoothed > 0) {
            m_window->statusBar()->showMessage(MainWindow::tr("Simplified %1 curve(s)").arg(smoothed), 3000);
        } else {
            m_window->statusBar()->showMessage(MainWindow::tr("No curves need simplifying"), 3000);
        }
    });
    m_window->m_selectAllAct = new QAction(MainWindow::tr("Select all items"), m_window);
    m_window->m_selectAllAct->setShortcut(QKeySequence::SelectAll);
    m_window->m_selectAllAct->setStatusTip(MainWindow::tr("Select all items (Ctrl+A)"));
    m_window->m_rotateCwAct = MakeAction(m_window, IconId::ROTATE_CW, MainWindow::tr("Rotate 45° clockwise"),
                                         QKeySequence(QStringLiteral("Ctrl+R")),
                                         MainWindow::tr("Rotate selected items 45° clockwise (Ctrl+R)"));
    m_window->m_rotateCcwAct = MakeAction(m_window, IconId::ROTATE_CCW, MainWindow::tr("Rotate 45° counterclockwise"),
                                          QKeySequence(QStringLiteral("Ctrl+Shift+R")),
                                          MainWindow::tr("Rotate selected items 45° counterclockwise (Ctrl+Shift+R)"));
    // 镜像翻转：PPT 的 flipH / flipV 语义，绕选区并集框中线翻转，参数值不变。
    m_window->m_mirrorHorizontalAct = MakeAction(m_window, IconId::MIRROR_H, MainWindow::tr("Mirror horizontally"),
                                                 QKeySequence(QStringLiteral("Ctrl+Alt+H")),
                                                 MainWindow::tr("Mirror selected items left↔right (Ctrl+Alt+H)"));
    m_window->m_mirrorHorizontalAct->setObjectName(QStringLiteral("MirrorHorizontalAction"));
    m_window->m_mirrorVerticalAct = MakeAction(m_window, IconId::MIRROR_V, MainWindow::tr("Mirror vertically"),
                                               QKeySequence(QStringLiteral("Ctrl+Alt+V")),
                                               MainWindow::tr("Mirror selected items top↕bottom (Ctrl+Alt+V)"));
    m_window->m_mirrorVerticalAct->setObjectName(QStringLiteral("MirrorVerticalAction"));

    m_window->m_viewModeAct =
        MakeAction(m_window, IconId::VIEW, MainWindow::tr("View mode"), QKeySequence(QStringLiteral("H")),
                   MainWindow::tr("View mode: overview, pan and zoom (H)"));
    m_window->m_brushModeAct =
        MakeAction(m_window, IconId::BRUSH, MainWindow::tr("Brush mode"), QKeySequence(QStringLiteral("B")),
                   MainWindow::tr("Brush mode: hold the left button to draw freely (B)"));
    m_window->m_editModeAct =
        MakeAction(m_window, IconId::EDIT, MainWindow::tr("Edit mode"), QKeySequence(QStringLiteral("V")),
                   MainWindow::tr("Edit mode: items on visible, unlocked layers are editable · "
                                  "double-click to select only that item · "
                                  "all item properties live in the edit panel · "
                                  "Space / middle button to pan the view (V)"));
    m_window->m_textModeAct =
        MakeAction(m_window, IconId::TEXT, MainWindow::tr("Text mode"), QKeySequence(QStringLiteral("T")),
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
    m_window->m_viewModeAct->setData(static_cast<int>(ToolMode::VIEW));
    m_window->m_brushModeAct->setData(static_cast<int>(ToolMode::BRUSH));
    m_window->m_editModeAct->setData(static_cast<int>(ToolMode::EDIT));
    m_window->m_textModeAct->setData(static_cast<int>(ToolMode::TEXT));

    m_window->m_fitAct = MakeAction(m_window, IconId::FIT, MainWindow::tr("Back to overview"),
                                    QKeySequence(QStringLiteral("Ctrl+0")), MainWindow::tr("Fit all items (Ctrl+0)"));
    m_window->m_zoomInAct = MakeAction(m_window, IconId::ZOOM_IN, MainWindow::tr("Zoom in"),
                                       QKeySequence(QStringLiteral("Ctrl+=")), MainWindow::tr("Zoom in (Ctrl+=)"));
    // 副键 Ctrl++（含小键盘加号同码）与 Ctrl+= 同属系统级放大标准绑定。
    m_window->m_zoomInAct->setShortcuts(
        {QKeySequence(QStringLiteral("Ctrl+=")), QKeySequence(QStringLiteral("Ctrl++"))});
    m_window->m_zoomOutAct = MakeAction(m_window, IconId::ZOOM_OUT, MainWindow::tr("Zoom out"),
                                        QKeySequence(QStringLiteral("Ctrl+-")), MainWindow::tr("Zoom out (Ctrl+-)"));
    m_window->m_actualSizeAct =
        MakeAction(m_window, IconId::ZOOM_IN, MainWindow::tr("Actual size"), QKeySequence(QStringLiteral("Ctrl+1")),
                   MainWindow::tr("Show at 100% (Ctrl+1)"));

    m_window->m_gridAct =
        MakeAction(m_window, IconId::GRID, MainWindow::tr("Helper grid"), QKeySequence(QStringLiteral("Ctrl+'")),
                   MainWindow::tr("Show or hide the helper grid on the canvas"));
    m_window->m_gridAct->setObjectName(QStringLiteral("GridToggleAction"));
    m_window->m_gridAct->setCheckable(true);
    m_window->m_gridAct->setChecked(m_window->m_settings.showGrid);
    QObject::connect(m_window->m_gridAct, &QAction::toggled, m_window, [this](bool visible) {
        m_window->m_scene->SetShowGrid(visible);
        m_window->m_settings.showGrid = visible;
        m_window->m_settings.Save();
        m_window->statusBar()->showMessage(
            visible ? MainWindow::tr("Helper grid shown") : MainWindow::tr("Helper grid hidden"), 2000);
    });

    // F11 全屏：全屏期菜单栏与工具栏都藏起来，键位由 FullScreenController 重新挂到画布视图，
    // 所以这条动作本身也必须能被保活，否则全屏后就退不出来。
    m_window->m_fullScreenAct =
        MakeAction(m_window, IconId::FULLSCREEN, MainWindow::tr("Full screen"), QKeySequence(QStringLiteral("F11")),
                   MainWindow::tr("Canvas only with a floating toolbar (F11)"));
    m_window->m_fullScreenAct->setObjectName(QStringLiteral("FullScreenAction"));
    m_window->m_fullScreenAct->setCheckable(true);
    QObject::connect(m_window->m_fullScreenAct, &QAction::triggered, m_window, &MainWindow::ToggleFullScreen);

    m_window->m_replaceBgAct =
        MakeAction(m_window, IconId::BACKGROUND, MainWindow::tr("Replace canvas background image…"), QKeySequence(),
                   MainWindow::tr("Choose an image as the canvas background"));
    m_window->m_bgColorAct = MakeAction(m_window, IconId::PALETTE, MainWindow::tr("Set background color…"),
                                        QKeySequence(), MainWindow::tr("Use a solid color as the canvas background"));
    m_window->m_clearBgAct = MakeAction(m_window, IconId::DELETE, MainWindow::tr("Clear canvas background"),
                                        QKeySequence(), MainWindow::tr("Restore a transparent background"));

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
        MakeAction(m_window, IconId::SETTINGS, MainWindow::tr("Settings…"), QKeySequence(QStringLiteral("Ctrl+,")),
                   MainWindow::tr("Open board settings (Ctrl+,)"));

    m_window->m_aboutAct =
        new QAction(QIcon(QStringLiteral(":/icons/app.png")), MainWindow::tr("About StoryBoard"), m_window);
    m_window->m_shortcutsAct = new QAction(MainWindow::tr("Keyboard shortcuts…"), m_window);
    m_window->m_shortcutsAct->setShortcut(QKeySequence(QKeySequence::HelpContents));
    m_window->m_shortcutsAct->setStatusTip(MainWindow::tr("List every action that has a shortcut"));
    m_window->m_aboutQtAct = new QAction(MainWindow::tr("About Qt"), m_window);

    QObject::connect(m_window->m_newAct, &QAction::triggered, m_window, &MainWindow::NewCanvas);
    QObject::connect(m_window->m_openAct, &QAction::triggered, m_window, &MainWindow::OpenCanvasDialog);
    QObject::connect(m_window->m_saveAct, &QAction::triggered, m_window, [this]() { m_window->SaveCanvas(); });
    QObject::connect(m_window->m_saveAsAct, &QAction::triggered, m_window, [this]() { m_window->SaveCanvasAs(); });
    QObject::connect(m_window->m_importAct, &QAction::triggered, m_window, &MainWindow::ImportImagesDialog);
    QObject::connect(m_window->m_lineArtAct, &QAction::triggered, m_window, &MainWindow::ImportOutlineImageDialog);
    QObject::connect(m_window->m_exportAct, &QAction::triggered, m_window, &MainWindow::ExportCanvas);
    QObject::connect(m_window->m_exportExcalidrawAct, &QAction::triggered, m_window, &MainWindow::ExportExcalidraw);
    QObject::connect(m_window->m_exportCustomShapeAct, &QAction::triggered, m_window, &MainWindow::ExportCustomShape);
    QObject::connect(m_window->m_quitAct, &QAction::triggered, m_window, &QWidget::close);

    QObject::connect(m_window->m_undoAct, &QAction::triggered, m_window->m_undo, &QUndoStack::undo);
    QObject::connect(m_window->m_redoAct, &QAction::triggered, m_window->m_undo, &QUndoStack::redo);
    QObject::connect(m_window->m_mirrorHorizontalAct, &QAction::triggered, m_window,
                     [this]() { m_window->m_scene->MirrorSelected(true); });
    QObject::connect(m_window->m_mirrorVerticalAct, &QAction::triggered, m_window,
                     [this]() { m_window->m_scene->MirrorSelected(false); });
    QObject::connect(m_window->m_deleteAct, &QAction::triggered, m_window->m_scene, &CanvasScene::DeleteSelected);
    QObject::connect(m_window->m_copyAct, &QAction::triggered, m_window,
                     [this]() { m_window->m_editActions->CopySelectedItems(); });
    QObject::connect(m_window->m_cutAct, &QAction::triggered, m_window,
                     [this]() { m_window->m_editActions->CutSelectedItems(); });
    QObject::connect(m_window->m_pasteAct, &QAction::triggered, m_window,
                     [this]() { m_window->m_editActions->PasteItems(); });
    QObject::connect(m_window->m_selectAllAct, &QAction::triggered, m_window->m_scene, &CanvasScene::SelectAllItems);
    QObject::connect(m_window->m_rotateCwAct, &QAction::triggered, m_window,
                     [this]() { m_window->RotateSelected(45.0); });
    QObject::connect(m_window->m_rotateCcwAct, &QAction::triggered, m_window,
                     [this]() { m_window->RotateSelected(-45.0); });

    QObject::connect(m_window->m_modeGroup, &QActionGroup::triggered, m_window,
                     [this](QAction* action) { m_window->SetToolMode(static_cast<ToolMode>(action->data().toInt())); });
    QObject::connect(m_window->m_fitAct, &QAction::triggered, m_window->m_view, &CanvasView::FitAll);
    QObject::connect(m_window->m_zoomInAct, &QAction::triggered, m_window->m_view, &CanvasView::ZoomIn);
    QObject::connect(m_window->m_zoomOutAct, &QAction::triggered, m_window->m_view, &CanvasView::ZoomOut);
    QObject::connect(m_window->m_actualSizeAct, &QAction::triggered, m_window->m_view, &CanvasView::ActualSize);

    QObject::connect(m_window->m_replaceBgAct, &QAction::triggered, m_window,
                     &MainWindow::ReplaceCanvasBackgroundImage);
    QObject::connect(m_window->m_bgColorAct, &QAction::triggered, m_window, &MainWindow::SetBackgroundColor);
    QObject::connect(m_window->m_clearBgAct, &QAction::triggered, m_window, &MainWindow::ClearBackground);
    QObject::connect(m_window->m_settingsAct, &QAction::triggered, m_window, &MainWindow::ShowSettings);

    m_window->m_fillStyleGroup = new QActionGroup(m_window);
    m_window->m_fillStyleGroup->setExclusive(true);
    for (const FillStyleMeta::Entry& entry : FillStyleMeta::Registry()) {
        auto* action = new QAction(MainWindow::tr(entry.actionSource.toLatin1().constData()), m_window);
        action->setCheckable(true);
        m_window->m_fillStyleActions.append(action);
        m_window->m_fillStyleGroup->addAction(action);
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, style = entry.style]() { m_window->SetFillStyle(style); });
    }
    // 需求 16：默认“无填充”，勾选状态必须按当前全局状态刷新，不能写死纯色。
    m_window->UpdateFillActions();
    m_window->m_fillColorAct = new QAction(MainWindow::tr("Fill color…"), m_window);
    m_window->m_fillColorAct->setObjectName(QStringLiteral("FillColorAction"));
    m_window->m_fillColorAct->setIcon(m_window->m_style->FillColorIcon(
        m_window->m_fillColor.isValid() ? m_window->m_fillColor : QColor(30, 41, 59, 36), 16));
    QObject::connect(m_window->m_fillColorAct, &QAction::triggered, m_window, &MainWindow::ChooseFillColor);
    m_window->m_strokeColorAct = new QAction(MainWindow::tr("Border color…"), m_window);
    m_window->m_strokeColorAct->setObjectName(QStringLiteral("BorderColorAction"));
    m_window->m_strokeColorAct->setIcon(m_window->m_style->StrokeColorIcon(m_window->m_brushColor, 16));
    QObject::connect(m_window->m_strokeColorAct, &QAction::triggered, m_window, &MainWindow::ChooseStrokeColor);

    // 需求 1/15：描线样式动作组，候选表与编辑卡、属性窗共用 ItemStyleVisuals::BorderStyles()。
    m_window->m_strokeStyleGroup = new QActionGroup(m_window);
    m_window->m_strokeStyleGroup->setExclusive(true);
    for (Qt::PenStyle penStyle : ItemStyleVisuals::BorderStyles()) {
        QAction* action = new QAction(ItemStyleVisuals::BorderStyleName(penStyle), m_window);
        action->setCheckable(true);
        action->setData(static_cast<int>(penStyle));
        m_window->m_strokeStyleGroup->addAction(action);
        m_window->m_strokeStyleActions.append(action);
        QObject::connect(action, &QAction::triggered, m_window,
                         [this, penStyle]() { m_window->SetStrokeStyle(penStyle); });
    }
    m_window->m_style->UpdateStrokeStyleActions();

    // 需求 1：菜单里的“描线宽度”改成普通菜单项，触发后把焦点放到工具栏宽度输入框末尾。
    m_window->m_strokeWidthFocusAct = new QAction(MainWindow::tr("Border width"), m_window);
    m_window->m_strokeWidthFocusAct->setObjectName(QStringLiteral("StrokeWidthFocusAction"));
    m_window->m_strokeWidthFocusAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    m_window->m_strokeWidthFocusAct->setShortcutContext(Qt::ApplicationShortcut);
    QObject::connect(m_window->m_strokeWidthFocusAct, &QAction::triggered, m_window, &MainWindow::FocusStrokeWidthSpin);

    auto setCanvasBackgroundMode = [this](BackgroundMode mode) {
        CanvasBackground background = m_window->m_scene->canvasBackground();
        if (background.type != BackgroundType::IMAGE) {
            return;
        }
        background.mode = mode;
        m_window->m_scene->ReplaceCanvasBackground(background, MainWindow::tr("Adjust background fill mode"));
    };
    QObject::connect(m_window->m_bgStretchAct, &QAction::triggered, m_window,
                     [setCanvasBackgroundMode]() { setCanvasBackgroundMode(BackgroundMode::STRETCH); });
    QObject::connect(m_window->m_bgFitAct, &QAction::triggered, m_window,
                     [setCanvasBackgroundMode]() { setCanvasBackgroundMode(BackgroundMode::FIT); });
    QObject::connect(m_window->m_bgTileAct, &QAction::triggered, m_window,
                     [setCanvasBackgroundMode]() { setCanvasBackgroundMode(BackgroundMode::TILE); });

    QObject::connect(m_window->m_aboutAct, &QAction::triggered, m_window, &MainWindow::ShowAbout);
    QObject::connect(m_window->m_shortcutsAct, &QAction::triggered, m_window, &MainWindow::ShowShortcuts);
    QObject::connect(m_window->m_aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
}
