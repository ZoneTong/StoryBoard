#include "ui/widgets/ViewStatusController.h"

#include "app/DockPanelController.h"
#include "canvas/CanvasView.h"
#include "canvas/controllers/CanvasBackgroundController.h"
#include "ops/Alignment.h"
#include "ops/BoolOps.h"
#include "ops/LineSplitOps.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/mainwindow/MainWindowEditActionsController.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QMimeData>
#include <QToolButton>
#include <QUndoStack>
#include <QtMath>

ViewStatusController::ViewStatusController(MainWindow* window) : m_window(window)
{
    // 外部应用复制后不会触发画布事件；剪贴板变化时主动刷新粘贴动作。
    QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, m_window, [this]() { UpdateActions(); });
}

void ViewStatusController::SetToolMode(ToolMode mode)
{
    m_window->m_view->SetMode(mode);
    UpdateModeUi();
}

void ViewStatusController::UpdateModeUi()
{
    if (!m_window->m_view) {
        return;
    }

    const ToolMode mode = m_window->m_view->mode();
    m_window->m_viewModeAct->setChecked(mode == ToolMode::VIEW);
    m_window->m_brushModeAct->setChecked(mode == ToolMode::BRUSH);
    m_window->m_editModeAct->setChecked(mode == ToolMode::EDIT);
    m_window->m_textModeAct->setChecked(mode == ToolMode::TEXT);

    if (m_window->m_modeBadge) {
        m_window->m_modeBadge->setText(ToolModeName(mode));
    }

    if (m_window->m_hintLabel) {
        switch (mode) {
        case ToolMode::VIEW:
            m_window->m_hintLabel->setText(
                MainWindow::tr("View mode: drag to pan · wheel to zoom · switching modes returns to the overview"));
            break;
        case ToolMode::BRUSH:
            m_window->m_hintLabel->setText(
                MainWindow::tr("Brush mode: left button to draw · wheel to zoom · Space / middle button to pan"));
            break;
        case ToolMode::EDIT:
            // 需求 1：只讲“哪些图元可编辑 + 双击精确单选 + 属性在编辑面板里改 + 唯一一条反直觉视图操作”，
            // 不再枚举拖动、缩放、滚轮、Delete 这类一看就会的操作，也不提 Ctrl（已改多选）。
            m_window->m_hintLabel->setText(MainWindow::tr("Edit mode: items on visible, unlocked layers are editable · "
                                                          "double-click to select only that item · "
                                                          "all item properties live in the edit panel · "
                                                          "Space / middle button to pan the view"));
            break;
        case ToolMode::TEXT:
            m_window->m_hintLabel->setText(
                MainWindow::tr("Text mode: click the canvas to place text · set font, size, color and bold/italic"));
            break;
        }
    }

    UpdateStatusBar();
    UpdateActions();
    UpdatePropertiesPanelDockVisibility();
}

void ViewStatusController::UpdateActions()
{
    if (!m_window->m_undo || !m_window->m_scene || !m_window->m_view) {
        return;
    }

    m_window->m_undoAct->setEnabled(m_window->m_undo->canUndo());
    m_window->m_redoAct->setEnabled(m_window->m_undo->canRedo());

    const bool editing = m_window->m_view->mode() == ToolMode::EDIT;
    m_window->m_deleteAct->setEnabled(editing && !m_window->m_scene->selectedItems().isEmpty());
    const bool hasSelection = !m_window->m_scene->SelectedCanvasItems().isEmpty();
    m_window->m_copyAct->setEnabled(editing && hasSelection);
    m_window->m_cutAct->setEnabled(editing && hasSelection);
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    m_window->m_pasteAct->setEnabled(editing && MainWindowEditActionsController::CanPaste(mime));
    m_window->m_duplicateAct->setEnabled(editing && hasSelection);
    m_window->m_groupAct->setEnabled(editing && m_window->m_scene->CanGroupSelectedItems());
    m_window->m_ungroupAct->setEnabled(editing && m_window->m_scene->CanUngroupSelectedItems());
    m_window->m_smoothAct->setEnabled(editing && m_window->m_scene->CanSmoothSelectedStrokes());
    m_window->m_selectAllAct->setEnabled(editing && m_window->m_scene->CanvasItemCount() > 0);
    m_window->m_rotateCwAct->setEnabled(editing && !m_window->m_scene->SelectedCanvasItems().isEmpty());
    m_window->m_rotateCcwAct->setEnabled(editing && !m_window->m_scene->SelectedCanvasItems().isEmpty());

    // 对齐动作按当前选择更新可用状态：对齐要 2 个图元，等间距要 3 个。
    bool alignAny = false;
    for (QAction* action : std::as_const(m_window->m_alignActions)) {
        const Alignment::Mode mode = static_cast<Alignment::Mode>(action->property("alignMode").toInt());
        const bool usable = editing && Alignment::Supports(m_window->m_scene, mode);
        action->setEnabled(usable);
        alignAny = alignAny || usable;
    }
    if (m_window->m_alignButton) {
        m_window->m_alignButton->setEnabled(alignAny);
    }

    // 布尔运算要选中至少两个可编辑图元；线条分割是填充图元 + 恰一条切刀，
    // 按自己的参与者规则单独判定启用状态。
    const bool boolReady = editing && BoolOps::Supports(m_window->m_scene);
    const bool lineSplitReady = editing && LineSplitOps::Supports(m_window->m_scene);
    for (QAction* action : std::as_const(m_window->m_boolActions)) {
        action->setEnabled(action->property("lineSplitAction").toBool() ? lineSplitReady : boolReady);
    }

    UpdateBackgroundModeActions();
}

void ViewStatusController::UpdateBackgroundModeActions()
{
    if (m_window->m_background) {
        m_window->m_background->UpdateModeActions();
    }
}

void ViewStatusController::UpdateStatusBar()
{
    if (!m_window->m_view || !m_window->m_scene) {
        return;
    }

    if (m_window->m_modeBadge) {
        m_window->m_modeBadge->setText(ToolModeName(m_window->m_view->mode()));
    }
    if (m_window->m_zoomBadge) {
        m_window->m_zoomBadge->setText(QStringLiteral("%1%").arg(qRound(m_window->m_view->Zoom() * 100.0)));
    }
    if (m_window->m_countBadge) {
        m_window->m_countBadge->setText(MainWindow::tr("%1 items · %2 layers")
                                            .arg(m_window->m_scene->CanvasItemCount())
                                            .arg(m_window->m_scene->UserLayerCount()));
    }
}

void ViewStatusController::FitAll()
{
    m_window->m_view->FitAll();
}

void ViewStatusController::UpdatePropertiesPanelDockVisibility()
{
    if (m_window->m_docks) {
        m_window->m_docks->UpdatePropertiesPanelDockVisibility();
    }
}
