#include "MainWindowEditActionsController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"
#include "ExcalidrawFormat.h"
#include "ItemPropertiesDialog.h"
#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QMimeData>
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
    const bool editable = m_window->m_view->mode() == ToolMode::EDIT && m_window->m_scene->isItemEditable(item);

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
    if (m_window->m_view->mode() != ToolMode::EDIT) {
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
    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->statusBar()->showMessage(MainWindow::tr("Alignment is available in edit mode only"), 3000);
        return;
    }
    QString commandText;
    switch (mode) {
    case Alignment::Mode::LEFT:
        commandText = MainWindow::tr("Align items left");
        break;
    case Alignment::Mode::HORIZONTAL_CENTER:
        commandText = MainWindow::tr("Align items horizontal centers");
        break;
    case Alignment::Mode::RIGHT:
        commandText = MainWindow::tr("Align items right");
        break;
    case Alignment::Mode::TOP:
        commandText = MainWindow::tr("Align items top");
        break;
    case Alignment::Mode::VERTICAL_CENTER:
        commandText = MainWindow::tr("Align items vertical centers");
        break;
    case Alignment::Mode::BOTTOM:
        commandText = MainWindow::tr("Align items bottom");
        break;
    case Alignment::Mode::DISTRIBUTE_HORIZONTAL:
        commandText = MainWindow::tr("Distribute items horizontally");
        break;
    case Alignment::Mode::DISTRIBUTE_VERTICAL:
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
    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->statusBar()->showMessage(MainWindow::tr("Merge items is available in edit mode only"), 3000);
        return;
    }
    QString commandText;
    switch (operation) {
    case BoolOps::Operation::UNION:
        commandText = MainWindow::tr("Union shapes");
        break;
    case BoolOps::Operation::XOR:
        commandText = MainWindow::tr("Xor shapes");
        break;
    case BoolOps::Operation::FRAGMENT:
        commandText = MainWindow::tr("Fragment shapes");
        break;
    case BoolOps::Operation::INTERSECT:
        commandText = MainWindow::tr("Intersect shapes");
        break;
    case BoolOps::Operation::SUBTRACT:
        commandText = MainWindow::tr("Subtract shapes");
        break;
    case BoolOps::Operation::LINE_SPLIT:
        commandText = MainWindow::tr("Split along line");
        break;
    }
    if (!BoolOps::apply(m_window->m_scene, operation, commandText)) {
        // 线条分割的常见失败是切刀没碰到填充图形，提示比通用文案更具体。
        if (operation == BoolOps::Operation::LINE_SPLIT) {
            m_window->statusBar()->showMessage(
                MainWindow::tr("Select filled shapes and exactly one line or curve that crosses them"), 3000);
            return;
        }
        m_window->statusBar()->showMessage(MainWindow::tr("Select at least two shapes with a visible outline"), 3000);
        return;
    }
    if (operation == BoolOps::Operation::LINE_SPLIT) {
        m_window->statusBar()->showMessage(MainWindow::tr("Split along the line (undo to restore)"), 3000);
    } else {
        m_window->statusBar()->showMessage(MainWindow::tr("Merged into one polygon (undo to restore)"), 3000);
    }
    m_window->updateActions();
}

void MainWindowEditActionsController::copySelectedItems()
{
    if (!m_window->m_scene) {
        return;
    }
    const QJsonArray items = m_window->m_scene->clipboardItems();
    if (items.isEmpty()) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select an item to copy"), 3000);
        return;
    }
    auto* mime = new QMimeData;
    mime->setData(QLatin1String(CLIPBOARD_MIME_TYPE), QJsonDocument(items).toJson(QJsonDocument::Compact));
    QApplication::clipboard()->setMimeData(mime);
    m_window->statusBar()->showMessage(MainWindow::tr("Copied %1 item(s)").arg(items.size()), 3000);
}

void MainWindowEditActionsController::cutSelectedItems()
{
    copySelectedItems();
    if (m_window->m_scene) {
        m_window->m_scene->deleteSelected();
    }
}

void MainWindowEditActionsController::pasteItems()
{
    if (!m_window->m_scene) {
        return;
    }
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    // 需求 9：从 excalidraw.com 复制的 JSON 也能直接粘到画板里。
    if (!mime->hasFormat(QLatin1String(CLIPBOARD_MIME_TYPE))) {
        const QJsonDocument external = QJsonDocument::fromJson(mime->data(QStringLiteral("text/plain")));
        if (external.isObject() && Excalidraw::isClipboard(external.object())) {
            QStringList notes;
            const QJsonArray items =
                Excalidraw::elementsToItems(external.object().value(QStringLiteral("elements")).toArray(), &notes);
            const int pastedCount = m_window->m_scene->pasteClipboardItems(items);
            if (pastedCount == 0) {
                m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
                return;
            }
            m_window->statusBar()->showMessage(MainWindow::tr("Pasted %1 Excalidraw item(s)").arg(pastedCount), 3000);
            return;
        }
    }
    if (!mime->hasFormat(QLatin1String(CLIPBOARD_MIME_TYPE))) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(mime->data(QLatin1String(CLIPBOARD_MIME_TYPE)));
    const int pasted = m_window->m_scene->pasteClipboardItems(doc.array());
    if (pasted == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Pasted %1 item(s)").arg(pasted), 3000);
}
