#include "CanvasItemInteraction.h"

#include "CanvasItem.h"
#include "CanvasScene.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QSet>
#include <QtMath>
#include <utility>

namespace {
constexpr qreal kMinimumSide = 8.0;

bool isLeftHandle(int handle)
{
    return handle == CanvasItem::TopLeft || handle == CanvasItem::Left || handle == CanvasItem::BottomLeft;
}

bool isRightHandle(int handle)
{
    return handle == CanvasItem::TopRight || handle == CanvasItem::Right || handle == CanvasItem::BottomRight;
}

bool isTopHandle(int handle)
{
    return handle == CanvasItem::TopLeft || handle == CanvasItem::Top || handle == CanvasItem::TopRight;
}

bool isBottomHandle(int handle)
{
    return handle == CanvasItem::BottomLeft || handle == CanvasItem::Bottom || handle == CanvasItem::BottomRight;
}

bool isCornerHandle(int handle)
{
    return handle == CanvasItem::TopLeft || handle == CanvasItem::TopRight || handle == CanvasItem::BottomLeft ||
           handle == CanvasItem::BottomRight;
}
} // namespace

CanvasItemInteraction::CanvasItemInteraction(CanvasItem* item) : m_item(item) {}

void CanvasItemInteraction::beginMove(const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::Move;
    m_item->m_dragStartScene = scenePos;
    m_item->m_dragStartPos = m_item->pos();
    m_item->m_dragStartSize = m_item->m_size;
    m_item->m_before.clear();

    if (m_item->scene()) {
        QSet<CanvasItem*> seen;
        QSet<int> groupIds;
        auto addItem = [&seen](CanvasItem* item) {
            if (item && !seen.contains(item)) {
                seen.insert(item);
            }
        };
        const QList<QGraphicsItem*> selected = m_item->scene()->selectedItems();
        for (QGraphicsItem* item : selected) {
            auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(item);
            if (!canvasItem) {
                continue;
            }
            if (!seen.contains(canvasItem)) {
                seen.insert(canvasItem);
            }
            if (canvasItem->groupId() > 0) {
                groupIds.insert(canvasItem->groupId());
            }
        }
        seen.insert(m_item);

        // 拖动任意一个组员时，把同一棵组树里的图元一并收进本次移动快照；组员不必处于
        // 选中状态，这样单个图元仍可打开属性窗并保持组内联动。
        if (CanvasScene* canvas = m_item->canvasScene()) {
            for (int groupId : std::as_const(groupIds)) {
                for (CanvasItem* member : canvas->itemsInGroupTree(canvas->outermostGroup(groupId))) {
                    // 正常分组都同层；若图层后续被拆分或锁定，至少不要移动
                    // 当前不可编辑的组员，避免绕过图层锁定。
                    if (canvas->isItemEditable(member)) {
                        addItem(member);
                    }
                }
            }
        }

        for (CanvasItem* canvasItem : std::as_const(seen)) {
            m_item->m_before.append({canvasItem, canvasItem->pos(), canvasItem->contentSize()});
        }
    }
    if (m_item->m_before.isEmpty()) {
        m_item->m_before.append({m_item, m_item->pos(), m_item->m_size});
    }
}

void CanvasItemInteraction::applyMove(const QPointF& scenePos)
{
    const QPointF delta = scenePos - m_item->m_dragStartScene;
    for (const ItemGeometry& geometry : std::as_const(m_item->m_before)) {
        if (geometry.item) {
            geometry.item->setPos(geometry.pos + delta);
        }
    }
}

void CanvasItemInteraction::beginResize(int handle, const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::Resize;
    m_item->m_resizeHandle = static_cast<CanvasItem::Handle>(handle);
    m_item->m_dragStartScene = scenePos;
    m_item->m_dragStartPos = m_item->pos();
    m_item->m_dragStartSize = m_item->m_size;
    m_item->m_before = {{m_item, m_item->pos(), m_item->m_size}};
}

void CanvasItemInteraction::applyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
{
    const QPointF delta = scenePos - m_item->m_dragStartScene;
    QRectF rect(m_item->m_dragStartPos, m_item->m_dragStartSize);
    const int handle = m_item->m_resizeHandle;

    if (isLeftHandle(handle)) {
        rect.setLeft(rect.left() + delta.x());
    }
    if (isRightHandle(handle)) {
        rect.setRight(rect.right() + delta.x());
    }
    if (isTopHandle(handle)) {
        rect.setTop(rect.top() + delta.y());
    }
    if (isBottomHandle(handle)) {
        rect.setBottom(rect.bottom() + delta.y());
    }

    const qreal minSide = kMinimumSide;
    if (rect.width() < minSide) {
        if (isLeftHandle(handle)) {
            rect.setLeft(rect.right() - minSide);
        } else {
            rect.setRight(rect.left() + minSide);
        }
    }
    if (rect.height() < minSide) {
        if (isTopHandle(handle)) {
            rect.setTop(rect.bottom() - minSide);
        } else {
            rect.setBottom(rect.top() + minSide);
        }
    }

    if ((modifiers & Qt::ShiftModifier) && isCornerHandle(handle) && m_item->m_dragStartSize.height() > 0.0) {
        const qreal aspect = m_item->m_dragStartSize.width() / m_item->m_dragStartSize.height();
        QSizeF size = rect.size();
        if (size.width() / size.height() > aspect) {
            size.setWidth(size.height() * aspect);
        } else {
            size.setHeight(size.width() / aspect);
        }
        size = size.expandedTo(QSizeF(minSide, minSide));

        if (isLeftHandle(handle)) {
            rect.setLeft(rect.right() - size.width());
        } else {
            rect.setRight(rect.left() + size.width());
        }
        if (isTopHandle(handle)) {
            rect.setTop(rect.bottom() - size.height());
        } else {
            rect.setBottom(rect.top() + size.height());
        }
    }

    m_item->setPos(rect.topLeft());
    m_item->setContentSize(rect.size());
}

void CanvasItemInteraction::beginRotate(const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::Rotate;
    m_item->m_dragStartScene = scenePos;
    m_item->m_dragStartRotation = m_item->m_rotation;
    const QPointF center = m_item->mapToScene(m_item->contentRect().center());
    m_item->m_rotateStartAngle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    m_item->m_before.clear();
}

void CanvasItemInteraction::applyRotate(const QPointF& scenePos)
{
    const QPointF center = m_item->mapToScene(m_item->contentRect().center());
    const qreal angle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    const qreal target = m_item->m_dragStartRotation + (angle - m_item->m_rotateStartAngle);
    // 需求 35：旋转手柄按 5° 步长吸附。
    m_item->setRotationAngle(CanvasItem::snapRotation(target, 5.0));
}

void CanvasItemInteraction::finishInteraction()
{
    if (m_item->m_drag == CanvasItem::DragKind::None) {
        return;
    }

    const bool wasResize = m_item->m_drag == CanvasItem::DragKind::Resize;
    const bool wasRotate = m_item->m_drag == CanvasItem::DragKind::Rotate;
    m_item->m_drag = CanvasItem::DragKind::None;
    m_item->m_resizeHandle = CanvasItem::NoHandle;

    if (wasRotate) {
        if (CanvasScene* canvas = m_item->canvasScene()) {
            canvas->pushRotation(m_item, m_item->m_dragStartRotation, m_item->m_rotation, m_item->tr("Rotate item"));
        }
    } else if (!m_item->m_before.isEmpty()) {
        ItemGeometryList after;
        after.reserve(m_item->m_before.size());
        for (const ItemGeometry& geometry : std::as_const(m_item->m_before)) {
            if (geometry.item) {
                after.append({geometry.item, geometry.item->pos(), geometry.item->contentSize()});
            }
        }
        if (CanvasScene* canvas = m_item->canvasScene()) {
            canvas->pushGeometry(m_item->m_before, after,
                                 wasResize ? m_item->tr("Resize item") : m_item->tr("Move item"));
        }
    }
    m_item->m_before.clear();
}

void CanvasItemInteraction::onHoverMove(QGraphicsSceneHoverEvent* event)
{
    if (!m_item->isEditable()) {
        m_item->unsetCursor();
        m_item->QGraphicsObject::hoverMoveEvent(event);
        return;
    }
    updateHoverCursor(event->pos());
    m_item->QGraphicsObject::hoverMoveEvent(event);
}

void CanvasItemInteraction::onHoverLeave(QGraphicsSceneHoverEvent* event)
{
    m_item->unsetCursor();
    m_item->QGraphicsObject::hoverLeaveEvent(event);
}

void CanvasItemInteraction::updateHoverCursor(const QPointF& pos)
{
    if (!m_item->isEditable()) {
        m_item->unsetCursor();
        return;
    }
    if (m_item->hasSingleSelection()) {
        const CanvasItem::Handle handle = m_item->handleAt(pos);
        if (handle != CanvasItem::NoHandle) {
            m_item->setCursor(m_item->cursorForHandle(handle));
            return;
        }
    }
    m_item->setCursor(QCursor(Qt::OpenHandCursor));
}

void CanvasItemInteraction::onMousePress(QGraphicsSceneMouseEvent* event)
{
    if (!m_item->isEditable() || event->button() != Qt::LeftButton) {
        m_item->QGraphicsObject::mousePressEvent(event);
        return;
    }

    if (m_item->hasSingleSelection()) {
        const CanvasItem::Handle handle = m_item->handleAt(event->pos());
        if (handle == CanvasItem::Rotation) {
            beginRotate(event->scenePos());
            m_item->setCursor(QCursor(Qt::PointingHandCursor));
            event->accept();
            return;
        }
        if (handle != CanvasItem::NoHandle) {
            beginResize(handle, event->scenePos());
            m_item->setCursor(m_item->cursorForHandle(handle));
            event->accept();
            return;
        }
    }

    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (modifiers & Qt::ControlModifier) {
        m_item->setSelected(!m_item->isSelected());
        if (!m_item->isSelected()) {
            event->accept();
            return;
        }
    } else if (!m_item->isSelected()) {
        if (!(modifiers & Qt::ShiftModifier)) {
            m_item->scene()->clearSelection();
        }
        m_item->setSelected(true);
    }

    beginMove(event->scenePos());
    m_item->setCursor(QCursor(Qt::ClosedHandCursor));
    event->accept();
}

void CanvasItemInteraction::onMouseMove(QGraphicsSceneMouseEvent* event)
{
    if (m_item->m_drag == CanvasItem::DragKind::None) {
        m_item->QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    if (m_item->m_drag == CanvasItem::DragKind::Move) {
        applyMove(event->scenePos());
    } else if (m_item->m_drag == CanvasItem::DragKind::Resize) {
        applyResize(event->scenePos(), event->modifiers());
    } else if (m_item->m_drag == CanvasItem::DragKind::Rotate) {
        applyRotate(event->scenePos());
    }

    event->accept();
}

void CanvasItemInteraction::onMouseRelease(QGraphicsSceneMouseEvent* event)
{
    if (m_item->m_drag == CanvasItem::DragKind::None) {
        m_item->QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    finishInteraction();
    updateHoverCursor(event->pos());
    event->accept();
}
