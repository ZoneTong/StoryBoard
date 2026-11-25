#include "canvas/controllers/CanvasTextEditor.h"

#include "canvas/CanvasView.h"
#include "items/CanvasItem.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QKeyEvent>
#include <QPointF>
#include <QRectF>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextOption>
#include <QUndoStack>

namespace {

// 输入框比图元外框稍大一圈，边框不至于把文字压住。
constexpr int EDITOR_MARGIN = 4;

} // namespace

CanvasTextEditor::CanvasTextEditor(CanvasView* view, CanvasScene* scene) : QObject(view), m_view(view), m_scene(scene) {}

bool CanvasTextEditor::BeginEdit(TextItem* item)
{
    Commit();
    if (!item || !m_view || !m_scene) {
        return false;
    }
    m_item = item;
    m_edit = new QTextEdit(m_view->viewport());
    m_edit->setObjectName(QStringLiteral("CanvasTextEditor"));
    m_edit->setFrameStyle(QFrame::NoFrame);
    m_edit->setStyleSheet(QStringLiteral("QTextEdit { background: #FFFFFF; border: 1px solid #2563EB;"
                                         " border-radius: 2px; padding: 0 2px; }"));
    m_edit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 字体按图元字号 × 当前缩放换算成屏幕像素，看到的字与画布上的字一样大。
    QFont font = item->TextFont();
    const qreal zoom = m_view->Zoom();
    const int pixel = qMax(8, qRound(qMax<qreal>(1.0, font.pointSizeF()) * zoom));
    font.setPixelSize(pixel);
    m_edit->setFont(font);
    m_edit->setPlainText(item->TextContent());

    QRect box = m_view->mapFromScene(item->mapRectToScene(item->ContentRect())).boundingRect();
    box = box.adjusted(-EDITOR_MARGIN, -EDITOR_MARGIN, EDITOR_MARGIN, EDITOR_MARGIN);
    box.setWidth(qMax(box.width(), 160));
    box.setHeight(qMax(box.height(), pixel + 2 * EDITOR_MARGIN + 4));
    m_edit->setGeometry(box);
    m_edit->show();
    m_edit->raise();
    m_edit->installEventFilter(this);
    // 焦点与光标都落到正文末尾：新建文字图元后可以直接接着打字。
    m_edit->setFocus(Qt::OtherFocusReason);
    QTextCursor cursor = m_edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_edit->setTextCursor(cursor);
    return true;
}

void CanvasTextEditor::Commit()
{
    if (!m_edit || !m_item || m_closing) {
        return;
    }
    m_closing = true;
    const QString text = m_edit->toPlainText();
    TextItem* item = m_item.data();
    CloseEditor();
    if (!item) {
        m_closing = false;
        return;
    }
    if (text != item->TextContent() && m_scene && m_scene->UndoStack()) {
        TextItemState before = item->State();
        TextItemState after = before;
        after.text = text;
        after.baseSize = TextItem::Measure(text, after.font);
        after.size = after.baseSize;
        // 容器文字按容器宽度重新折行，撤销/重做也恢复同一份排版尺寸。
        if (!item->ContainerUid().isEmpty()) {
            CanvasItem* container = m_scene->ItemByUid(item->ContainerUid());
            if (container) {
                const QRectF rect = container->mapRectToScene(container->ContentRect());
                TextItem probe;
                probe.SetTextFormat(text, after.font, after.color);
                after.size = probe.ContainerLayoutSize(rect.width());
                after.pos = QPointF(rect.left(), rect.center().y() - after.size.height() / 2.0);
            }
        }
        m_scene->UndoStack()->push(new TextItemCommand(m_scene, item, before, after, tr("Edit text")));
    }
    if (m_view) {
        m_view->setFocus();
    }
    m_closing = false;
}

void CanvasTextEditor::Cancel()
{
    if (m_closing) {
        return;
    }
    m_closing = true;
    CloseEditor();
    if (m_view) {
        m_view->setFocus();
    }
    m_closing = false;
}

void CanvasTextEditor::CloseEditor()
{
    if (!m_edit) {
        return;
    }
    QTextEdit* edit = m_edit;
    m_edit = nullptr;
    m_item = nullptr;
    edit->removeEventFilter(this);
    edit->hide();
    edit->deleteLater();
}

bool CanvasTextEditor::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            Cancel();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Excalidraw 口径：回车提交，Shift+Enter 在正文里换行。
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false;
            }
            Commit();
            return true;
        }
    }
    if (watched == m_edit && event->type() == QEvent::FocusOut) {
        Commit();
    }
    return QObject::eventFilter(watched, event);
}
