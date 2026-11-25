#include "ui/mainwindow/MainWindowEditActionsController.h"

#include "canvas/CanvasView.h"
#include "io/ExcalidrawFormat.h"
#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "ui/mainwindow/MainWindow.h"

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

void MainWindowEditActionsController::RotateSelected(qreal degrees)
{
    if (m_window->View()->mode() != ToolMode::EDIT) {
        m_window->statusBar()->showMessage(MainWindow::tr("Rotation is available in edit mode only"), 3000);
        return;
    }
    const int changed =
        m_window->Scene()->RotateSelected(degrees, degrees > 0 ? MainWindow::tr("Rotate items clockwise")
                                                               : MainWindow::tr("Rotate items counterclockwise"));
    if (changed == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("No selected items can be rotated"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Rotated %n Item(s) by %1°", nullptr, changed).arg(qAbs(degrees)),
                                       3000);
}

void MainWindowEditActionsController::ApplyAlignment(Alignment::Mode mode)
{
    if (m_window->View()->mode() != ToolMode::EDIT) {
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
    const int moved = Alignment::Apply(m_window->Scene(), mode, commandText);
    if (moved == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("The selected items are already aligned"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Aligned %n Item(s)", nullptr, moved), 3000);
}

void MainWindowEditActionsController::ApplyBoolOperation(BoolOps::Operation operation)
{
    if (m_window->View()->mode() != ToolMode::EDIT) {
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
    if (!BoolOps::Apply(m_window->Scene(), operation, commandText)) {
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
    if (!m_window->Scene()) {
        return;
    }
    const QJsonArray items = m_window->Scene()->ClipboardItems();
    if (items.isEmpty()) {
        m_window->statusBar()->showMessage(MainWindow::tr("Select an item to copy"), 3000);
        return;
    }
    auto* mime = new QMimeData;
    mime->setData(QLatin1String(CLIPBOARD_MIME_TYPE), QJsonDocument(items).toJson(QJsonDocument::Compact));
    // 同时写一份 Excalidraw 剪贴板 JSON，复制后可直接粘到 excalidraw.com；
    // text/plain 兜底给只读纯文本的浏览器/终端。
    QJsonObject document;
    document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    document.insert(QStringLiteral("items"), items);
    QStringList warnings;
    const QJsonObject excalidrawClipboard = Excalidraw::StoryBoardToClipboard(document, &warnings);
    const QByteArray excalidrawJson = QJsonDocument(excalidrawClipboard).toJson(QJsonDocument::Compact);
    mime->setData(QStringLiteral("application/vnd.excalidraw+json"), excalidrawJson);
    mime->setText(QString::fromUtf8(excalidrawJson));
    QApplication::clipboard()->setMimeData(mime);
    m_window->statusBar()->showMessage(MainWindow::tr("Copied %n Item(s)", nullptr, items.size()), 3000);
}

void MainWindowEditActionsController::CutSelectedItems()
{
    CopySelectedItems();
    if (m_window->Scene()) {
        m_window->Scene()->DeleteSelected();
    }
}

bool MainWindowEditActionsController::CanPaste(const QMimeData* mime)
{
    if (!mime) {
        return false;
    }
    if (mime->hasFormat(QLatin1String(CLIPBOARD_MIME_TYPE))) {
        return true;
    }
    QByteArray data = mime->data(QStringLiteral("application/vnd.excalidraw+json"));
    if (data.isEmpty()) {
        data = mime->data(QStringLiteral("text/plain"));
    }
    const QJsonDocument document = QJsonDocument::fromJson(data);
    return document.isObject() && Excalidraw::IsClipboard(document.object());
}

void MainWindowEditActionsController::PasteItems()
{
    if (!m_window->Scene()) {
        return;
    }
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    // 需求 9：从 excalidraw.com 复制的 JSON 也能直接粘到画板里。
    if (!mime->hasFormat(QLatin1String(CLIPBOARD_MIME_TYPE))) {
        QByteArray externalData = mime->data(QStringLiteral("application/vnd.excalidraw+json"));
        if (externalData.isEmpty()) {
            externalData = mime->data(QStringLiteral("text/plain"));
        }
        const QJsonDocument external = QJsonDocument::fromJson(externalData);
        if (external.isObject() && Excalidraw::IsClipboard(external.object())) {
            QStringList notes;
            const QJsonObject scene = external.object();
            const QJsonArray items =
                Excalidraw::ElementsToItems(scene.value(QStringLiteral("elements")).toArray(), &notes, nullptr,
                                            scene.value(QStringLiteral("files")).toObject(), true);
            const int pastedCount = m_window->Scene()->PasteClipboardItems(items);
            if (pastedCount == 0) {
                m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
                return;
            }
            m_window->statusBar()->showMessage(MainWindow::tr("Pasted %n Excalidraw Item(s)", nullptr, pastedCount),
                                               3000);
            return;
        }
    }
    if (!mime->hasFormat(QLatin1String(CLIPBOARD_MIME_TYPE))) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(mime->data(QLatin1String(CLIPBOARD_MIME_TYPE)));
    const int pasted = m_window->Scene()->PasteClipboardItems(doc.array());
    if (pasted == 0) {
        m_window->statusBar()->showMessage(MainWindow::tr("Nothing to paste"), 3000);
        return;
    }
    m_window->statusBar()->showMessage(MainWindow::tr("Pasted %n Item(s)", nullptr, pasted), 3000);
}
