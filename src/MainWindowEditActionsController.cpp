#include "MainWindowEditActionsController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "ItemPropertiesDialog.h"
#include "MainWindow.h"

#include <QList>
#include <QPointer>
#include <QStatusBar>
#include <QtMath>

MainWindowEditActionsController::MainWindowEditActionsController(MainWindow* window) : m_window(window) {}

void MainWindowEditActionsController::showSelectedItemProperties()
{
    const QList<CanvasItem*> items = m_window->m_scene->selectedCanvasItems();
    if (items.size() != 1) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select a single item to show its properties"), 3000);
        return;
    }
    showItemProperties(items.constFirst());
}

void MainWindowEditActionsController::showItemProperties(CanvasItem* item)
{
    if (!item || item->scene() != m_window->m_scene) {
        return;
    }

    // 需求 42：属性窗在任何模式都能打开，但只有编辑模式下且图层可见未锁定
    // 时才允许修改；查看/画笔/文字模式一律以只读方式展示。
    const bool editable = m_window->m_view->mode() == ToolMode::Edit && m_window->m_scene->isItemEditable(item);

    closeItemProperties();
    ItemPropertiesDialog dialog(m_window->m_scene, item, m_window, editable);
    m_window->m_itemPropertiesDialog = &dialog;
    dialog.exec();
    m_window->m_itemPropertiesDialog = nullptr;
}

void MainWindowEditActionsController::closeItemProperties()
{
    if (!m_window->m_itemPropertiesDialog) {
        return;
    }
    QPointer<ItemPropertiesDialog> dialog = m_window->m_itemPropertiesDialog;
    m_window->m_itemPropertiesDialog = nullptr;
    if (dialog) {
        dialog->close();
    }
}

void MainWindowEditActionsController::rotateSelected(qreal degrees)
{
    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->statusBar()->showMessage(MainWindow::tr("Rotation is available in edit mode only"), 3000);
        return;
    }
    const int changed =
        m_window->m_scene->rotateSelected(degrees, degrees > 0 ? MainWindow::tr("Rotate items clockwise")
                                                               : MainWindow::tr("Rotate items counterclockwise"));
    if (changed == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("No selected items can be rotated"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Rotated %1 item(s) by %2°").arg(changed).arg(qAbs(degrees)),
                                       3000);
}

void MainWindowEditActionsController::applyAlignment(Alignment::Mode mode)
{
    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->statusBar()->showMessage(MainWindow::tr("Alignment is available in edit mode only"), 3000);
        return;
    }
    QString commandText;
    switch (mode) {
    case Alignment::Mode::Left:
        commandText = MainWindow::tr("Align items left");
        break;
    case Alignment::Mode::HorizontalCenter:
        commandText = MainWindow::tr("Align items horizontal centers");
        break;
    case Alignment::Mode::Right:
        commandText = MainWindow::tr("Align items right");
        break;
    case Alignment::Mode::Top:
        commandText = MainWindow::tr("Align items top");
        break;
    case Alignment::Mode::VerticalCenter:
        commandText = MainWindow::tr("Align items vertical centers");
        break;
    case Alignment::Mode::Bottom:
        commandText = MainWindow::tr("Align items bottom");
        break;
    case Alignment::Mode::DistributeHorizontal:
        commandText = MainWindow::tr("Distribute items horizontally");
        break;
    case Alignment::Mode::DistributeVertical:
        commandText = MainWindow::tr("Distribute items vertically");
        break;
    }
    const int moved = Alignment::apply(m_window->m_scene, mode, commandText);
    if (moved == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("The selected items are already aligned"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Aligned %1 item(s)").arg(moved), 3000);
}

void MainWindowEditActionsController::applyBoolOperation(BoolOps::Operation operation)
{
    if (m_window->m_view->mode() != ToolMode::Edit) {
        m_window->statusBar()->showMessage(MainWindow::tr("Merge items is available in edit mode only"), 3000);
        return;
    }
    QString commandText;
    switch (operation) {
    case BoolOps::Operation::Union:
        commandText = MainWindow::tr("Union shapes");
        break;
    case BoolOps::Operation::Combine:
        commandText = MainWindow::tr("Combine shapes and cut the overlap");
        break;
    case BoolOps::Operation::Fragment:
        commandText = MainWindow::tr("Fragment shapes");
        break;
    case BoolOps::Operation::Intersect:
        commandText = MainWindow::tr("Intersect shapes");
        break;
    case BoolOps::Operation::Subtract:
        commandText = MainWindow::tr("Subtract shapes");
        break;
    }
    if (!BoolOps::apply(m_window->m_scene, operation, commandText)) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select at least two shapes with a visible outline"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Merged into one shape (undo to restore)"), 3000);
    m_window->updateActions();
}
