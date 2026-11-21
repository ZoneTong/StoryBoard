#include "CanvasItemHandles.h"

#include "CanvasItem.h"
#include "ShapeItem.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal HANDLE_HIT_PADDING = 2.0;
} // namespace

CanvasItemHandles::CanvasItemHandles(CanvasItem* item) : m_item(item) {}

// 需求 2：单选三角形时顶点 C 取代 Top 句柄；其它情况仍用八向包围框。
bool CanvasItemHandles::UsesApexHandle() const
{
    if (!m_item || !m_item->isSelected() || !m_item->IsEditable() || !m_item->HasSingleSelection()) {
        return false;
    }
    const auto* shape = dynamic_cast<const ShapeItem*>(m_item);
    return shape && shape->ApexAngleHandle() && !shape->IsCustomShape();
}

qreal CanvasItemHandles::Size() const
{
    return qBound<qreal>(6.0, 9.0 / m_item->ViewScale(), 40.0);
}

qreal CanvasItemHandles::RotationOffset() const
{
    // Keep the rotation handle one handle-size plus a small gap above the frame,
    // so it stays visually separate from the Top resize handle at any zoom.
    return Size() + 16.0 / m_item->ViewScale();
}

QPointF CanvasItemHandles::Center(int handle) const
{
    const QRectF rect = m_item->FrameRect();
    switch (static_cast<CanvasItem::Handle>(handle)) {
    case CanvasItem::TOP_LEFT:
        return rect.topLeft();
    case CanvasItem::TOP:
        return QPointF(rect.center().x(), rect.top());
    case CanvasItem::TOP_RIGHT:
        return rect.topRight();
    case CanvasItem::RIGHT:
        return QPointF(rect.right(), rect.center().y());
    case CanvasItem::BOTTOM_RIGHT:
        return rect.bottomRight();
    case CanvasItem::BOTTOM:
        return QPointF(rect.center().x(), rect.bottom());
    case CanvasItem::BOTTOM_LEFT:
        return rect.bottomLeft();
    case CanvasItem::LEFT:
        return QPointF(rect.left(), rect.center().y());
    case CanvasItem::ROTATION:
        return QPointF(rect.center().x(), rect.top() - RotationOffset());
    case CanvasItem::APEX: {
        bool ok = false;
        const QPointF custom = m_item->CustomHandleCenter(CanvasItem::APEX, &ok);
        if (ok) {
            return custom;
        }
        break;
    }
    case CanvasItem::NO_HANDLE:
        break;
    }
    return rect.center();
}

QRectF CanvasItemHandles::Rect(int handle) const
{
    const qreal handleSize = Size();
    const QPointF handleCenter = Center(handle);
    return QRectF(handleCenter.x() - handleSize / 2.0, handleCenter.y() - handleSize / 2.0, handleSize, handleSize);
}

int CanvasItemHandles::HitAt(const QPointF& pos) const
{
    const qreal hitPadding = qMin<qreal>(10.0 / m_item->ViewScale(), 14.0);
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && UsesApexHandle()) {
            continue;
        }
        const QRectF handleRect = Rect(handle).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (handleRect.contains(pos)) {
            return handle;
        }
    }
    if (UsesApexHandle()) {
        const QRectF handleRect = Rect(CanvasItem::APEX).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (handleRect.contains(pos)) {
            return CanvasItem::APEX;
        }
    }
    return CanvasItem::NO_HANDLE;
}

QPainterPath CanvasItemHandles::SelectionShape() const
{
    QPainterPath path;
    if (!m_item->isSelected() || !m_item->IsEditable() || !m_item->HasSingleSelection()) {
        return path;
    }

    // Selection handles are centred on the content corners, so half of every
    // handle lies outside the item geometry. Drawing them without adding them
    // to shape() leaves the visible handle unclickable; for outlines that never
    // reach the corner (ellipse, triangle, parallelogram, star) the handle could not
    // be grabbed at all.
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && UsesApexHandle()) {
            continue;
        }
        path.addRect(
            Rect(handle).adjusted(-HANDLE_HIT_PADDING, -HANDLE_HIT_PADDING, HANDLE_HIT_PADDING, HANDLE_HIT_PADDING));
    }
    if (UsesApexHandle()) {
        path.addRect(Rect(CanvasItem::APEX)
                         .adjusted(-HANDLE_HIT_PADDING, -HANDLE_HIT_PADDING, HANDLE_HIT_PADDING, HANDLE_HIT_PADDING));
    }
    return path;
}

void CanvasItemHandles::DrawOverlay(QPainter* painter) const
{
    QPen borderPen(QColor(79, 124, 255));
    borderPen.setWidthF(1.3);
    borderPen.setCosmetic(true);
    borderPen.setStyle(Qt::DashLine);
    borderPen.setDashPattern({5.0, 4.0});
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_item->FrameRect());

    if (!m_item->IsEditable() || !m_item->HasSingleSelection()) {
        return;
    }

    QPen handlePen(QColor(79, 124, 255));
    handlePen.setWidthF(1.4);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::LEFT; ++handle) {
        if (handle == CanvasItem::TOP && UsesApexHandle()) {
            continue;
        }
        painter->drawRect(Rect(handle));
    }
    if (UsesApexHandle()) {
        painter->drawRect(Rect(CanvasItem::APEX));
    }

    // Rotation handle: a circular knob above the frame, connected by a thin line.
    const QPointF topCenter(Center(CanvasItem::TOP).x(), m_item->FrameRect().top());
    const QPointF rotationCenter = Center(CanvasItem::ROTATION);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(QColor(79, 124, 255));
    painter->drawEllipse(Rect(CanvasItem::ROTATION).adjusted(1.0, 1.0, -1.0, -1.0));
}

QCursor CanvasItemHandles::Cursor(int handle) const
{
    switch (static_cast<CanvasItem::Handle>(handle)) {
    case CanvasItem::TOP_LEFT:
    case CanvasItem::BOTTOM_RIGHT:
        return QCursor(Qt::SizeFDiagCursor);
    case CanvasItem::TOP_RIGHT:
    case CanvasItem::BOTTOM_LEFT:
        return QCursor(Qt::SizeBDiagCursor);
    case CanvasItem::TOP:
    case CanvasItem::BOTTOM:
        return QCursor(Qt::SizeVerCursor);
    case CanvasItem::LEFT:
    case CanvasItem::RIGHT:
        return QCursor(Qt::SizeHorCursor);
    case CanvasItem::APEX:
        return QCursor(Qt::SizeAllCursor);
    case CanvasItem::ROTATION:
        return QCursor(Qt::PointingHandCursor);
    case CanvasItem::NO_HANDLE:
        break;
    }
    return QCursor(Qt::ArrowCursor);
}
