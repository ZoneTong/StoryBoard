#include "ui/mainwindow/MainWindowEditActionsController.h"

#include "canvas/CanvasView.h"
#include "io/ExcalidrawFormat.h"
#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"

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

void MainWindowEditActionsController::ShowSelectedItemProperties()
{
    const QList<CanvasItem*> items = m_window->m_scene->SelectedCanvasItems();
    if (items.size() != 1) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select a single item to show its properties"), 3000);
        return;
    }
    ShowItemProperties(items.constFirst());
}

void MainWindowEditActionsController::ShowItemProperties(CanvasItem* item)
{
    if (!item || item->scene() != m_window->m_scene) {
        return;
    }

    // 需求 42：属性窗在任何模式都能打开，但只有编辑模式下且图层可见未锁定
    // 时才允许修改；查看/画笔/文字模式一律以只读方式展示。
    const bool editable = m_window->m_view->mode() == ToolMode::EDIT && m_window->m_scene->IsItemEditable(item);

    // 所有图元类型复用同一个属性窗实例：行控件一次建齐、顺序固定，
    // 每次打开只换绑图元并按属性显隐，不再按类型重建对话框。
    ItemPropertiesDialog* dialog = m_window->m_itemPropertiesDialog;
    if (!dialog) {
        dialog = new ItemPropertiesDialog(nullptr, nullptr, m_window, true);
        m_window->m_itemPropertiesDialog = dialog;
    }
    dialog->SetItem(m_window->m_scene, item, editable);
    dialog->exec();
}

void MainWindowEditActionsController::CloseItemProperties()
{
    if (!m_window->m_itemPropertiesDialog) {
        return;
    }
    // 实例常驻复用：这里只负责关闭（取消未确认修改），不销毁。
    if (m_window->m_itemPropertiesDialog->isVisible()) {
        m_window->m_itemPropertiesDialog->close();
    }
}

void MainWindowEditActionsController::RotateSelected(qreal degrees)
{
    if (m_window->m_view->mode() != ToolMode::EDIT) {
        m_window->statusBar()->showMessage(MainWindow::tr("Rotation is available in edit mode only"), 3000);
        return;
    }
    const int changed =
        m_window->m_scene->RotateSelected(degrees, degrees > 0 ? MainWindow::tr("Rotate items clockwise")
                                                               : MainWindow::tr("Rotate items counterclockwise"));
    if (changed == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("No selected items can be rotated"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Rotated %1 Item(s) by %2°").arg(changed).arg(qAbs(degrees)),
                                       3000);
}

void MainWindowEditActionsController::ApplyAlignment(Alignment::Mode mode)
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
    const int moved = Alignment::Apply(m_window->m_scene, mode, commandText);
    if (moved == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("The selected items are already aligned"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Aligned %1 Item(s)").arg(moved), 3000);
}

void MainWindowEditActionsController::ApplyBoolOperation(BoolOps::Operation operation)
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
    if (!BoolOps::Apply(m_window->m_scene, operation, commandText)) {
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
    m_window->UpdateActions();
}

void MainWindowEditActionsController::CopySelectedItems()
{
    if (!m_window->m_scene) {
        return;
    }
    const QJsonArray items = m_window->m_scene->ClipboardItems();
    if (items.isEmpty()) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select an item to copy"), 3000);
        return;
    }
    auto* mime = new QMimeData;
    mime->setData(QLatin1String(CLIPBOARD_MIME_TYPE), QJsonDocument(items).toJson(QJsonDocument::Compact));
    QApplication::clipboard()->setMimeData(mime);
    m_window->statusBar()->showMessage(MainWindow::tr("Copied %1 Item(s)").arg(items.size()), 3000);
}

void MainWindowEditActionsController::CutSelectedItems()
{
    CopySelectedItems();
    if (m_window->m_scene) {
        m_window->m_scene->DeleteSelected();
    }
}

void MainWindowEditActionsController::PasteItems()
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
        if (external.isObject() && Excalidraw::IsClipboard(external.object())) {
            QStringList notes;
            const QJsonArray items =
                Excalidraw::ElementsToItems(external.object().value(QStringLiteral("elements")).toArray(), &notes);
            const int pastedCount = m_window->m_scene->PasteClipboardItems(items);
            if (pastedCount == 0) {
                m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
                return;
            }
            m_window->statusBar()->showMessage(MainWindow::tr("Pasted %1 Excalidraw Item(s)").arg(pastedCount), 3000);
            return;
        }
    }
    if (!mime->hasFormat(QLatin1String(CLIPBOARD_MIME_TYPE))) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(mime->data(QLatin1String(CLIPBOARD_MIME_TYPE)));
    const int pasted = m_window->m_scene->PasteClipboardItems(doc.array());
    if (pasted == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Pasted %1 Item(s)").arg(pasted), 3000);
}
