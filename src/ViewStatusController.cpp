#include "ViewStatusController.h"

#include "Alignment.h"
#include "BackgroundController.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "DockPanelController.h"
#include "MainWindow.h"
#include "MainWindowEditActionsController.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QMimeData>
#include <QToolButton>
#include <QUndoStack>
#include <QtMath>

ViewStatusController::ViewStatusController(MainWindow* window) : m_window(window) {}

void ViewStatusController::setToolMode(ToolMode mode)
{
    m_window->m_view->setMode(mode);
    updateModeUi();
}

void ViewStatusController::updateModeUi()
{
    if (!m_window->m_view) {
        return;
    }

    const ToolMode mode = m_window->m_view->mode();
    m_window->m_viewModeAct->setChecked(mode == ToolMode::View);
    m_window->m_brushModeAct->setChecked(mode == ToolMode::Brush);
    m_window->m_editModeAct->setChecked(mode == ToolMode::Edit);
    m_window->m_textModeAct->setChecked(mode == ToolMode::Text);

    if (m_window->m_modeBadge) {
        m_window->m_modeBadge->setText(toolModeName(mode));
    }

    if (m_window->m_hintLabel) {
        switch (mode) {
        case ToolMode::View:
            m_window->m_hintLabel->setText(
                MainWindow::tr("View mode: drag to pan · wheel to zoom · switching modes returns to the overview"));
            break;
        case ToolMode::Brush:
            m_window->m_hintLabel->setText(
                MainWindow::tr("Brush mode: left button to draw · wheel to zoom · Space / middle button to pan"));
            break;
        case ToolMode::Edit:
            // 需求 1：只讲“哪些图元可编辑 + 属性入口（双击）+ 唯一一条反直觉视图操作”，
            // 不再枚举拖动、缩放、滚轮、Delete 这类一看就会的操作，也不提 Ctrl（已改多选）。
            m_window->m_hintLabel->setText(MainWindow::tr("Edit mode: items on visible, unlocked layers are editable · "
                                                          "double-click to open item properties and change them · "
                                                          "Space / middle button to pan the view"));
            break;
        case ToolMode::Text:
            m_window->m_hintLabel->setText(
                MainWindow::tr("Text mode: click the canvas to place text · set font, size, color and bold/italic"));
            break;
        }
    }

    updateStatusBar();
    updateActions();
    updateItemDockVisibility();
}

void ViewStatusController::updateActions()
{
    if (!m_window->m_undo || !m_window->m_scene || !m_window->m_view) {
        return;
    }

    m_window->m_undoAct->setEnabled(m_window->m_undo->canUndo());
    m_window->m_redoAct->setEnabled(m_window->m_undo->canRedo());

    const bool editing = m_window->m_view->mode() == ToolMode::Edit;
    m_window->m_deleteAct->setEnabled(editing && !m_window->m_scene->selectedItems().isEmpty());
    const bool hasSelection = !m_window->m_scene->selectedCanvasItems().isEmpty();
    m_window->m_copyAct->setEnabled(editing && hasSelection);
    m_window->m_cutAct->setEnabled(editing && hasSelection);
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    m_window->m_pasteAct->setEnabled(
        editing && mime && mime->hasFormat(QLatin1String(MainWindowEditActionsController::kClipboardMimeType)));
    m_window->m_duplicateAct->setEnabled(editing && hasSelection);
    m_window->m_groupAct->setEnabled(editing && m_window->m_scene->canGroupSelectedItems());
    m_window->m_ungroupAct->setEnabled(editing && m_window->m_scene->canUngroupSelectedItems());
    m_window->m_smoothAct->setEnabled(editing && m_window->m_scene->canSmoothSelectedStrokes());
    m_window->m_selectAllAct->setEnabled(editing && m_window->m_scene->canvasItemCount() > 0);
    m_window->m_itemPropsAct->setEnabled(m_window->m_scene->selectedCanvasItems().size() == 1);
    m_window->m_rotateCwAct->setEnabled(editing && !m_window->m_scene->selectedCanvasItems().isEmpty());
    m_window->m_rotateCcwAct->setEnabled(editing && !m_window->m_scene->selectedCanvasItems().isEmpty());

    // 对齐动作按当前选择更新可用状态：对齐要 2 个图元，等间距要 3 个。
    bool alignAny = false;
    for (QAction* action : std::as_const(m_window->m_alignActions)) {
        const Alignment::Mode mode = static_cast<Alignment::Mode>(action->property("alignMode").toInt());
        const bool usable = editing && Alignment::supports(m_window->m_scene, mode);
        action->setEnabled(usable);
        alignAny = alignAny || usable;
    }
    if (m_window->m_alignButton) {
        m_window->m_alignButton->setEnabled(alignAny);
    }

    // 布尔运算要选中至少两个可编辑图元。
    const bool boolReady = editing && BoolOps::supports(m_window->m_scene);
    for (QAction* action : std::as_const(m_window->m_boolActions)) {
        action->setEnabled(boolReady);
    }
    if (m_window->m_boolButton) {
        m_window->m_boolButton->setEnabled(boolReady);
    }

    updateBackgroundModeActions();
}

void ViewStatusController::updateBackgroundModeActions()
{
    if (m_window->m_background) {
        m_window->m_background->updateModeActions();
    }
}

void ViewStatusController::updateStatusBar()
{
    if (!m_window->m_view || !m_window->m_scene) {
        return;
    }

    if (m_window->m_modeBadge) {
        m_window->m_modeBadge->setText(toolModeName(m_window->m_view->mode()));
    }
    if (m_window->m_zoomBadge) {
        m_window->m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(m_window->m_view->zoom() * 100.0)));
    }
    if (m_window->m_countBadge) {
        m_window->m_countBadge->setText(MainWindow::tr("%1 items · %2 layers")
                                            .arg(m_window->m_scene->canvasItemCount())
                                            .arg(m_window->m_scene->userLayerCount()));
    }
}

void ViewStatusController::fitAll()
{
    m_window->m_view->fitAll();
}

void ViewStatusController::updateItemDockVisibility()
{
    if (m_window->m_docks) {
        m_window->m_docks->updateItemDockVisibility();
    }
}
