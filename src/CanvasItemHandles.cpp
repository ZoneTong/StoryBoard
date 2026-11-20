#include "CanvasItemHandles.h"

#include "CanvasItem.h"
#include "ShapeItem.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal kHandleHitPadding = 2.0;
} // namespace

CanvasItemHandles::CanvasItemHandles(CanvasItem* item) : m_item(item) {}

// 需求 2：单选三角形时顶点 C 取代 Top 句柄；其它情况仍用八向包围框。
bool CanvasItemHandles::usesApexHandle() const
{
    if (!m_item || !m_item->isSelected() || !m_item->isEditable() || !m_item->hasSingleSelection()) {
        return false;
    }
    const auto* shape = dynamic_cast<const ShapeItem*>(m_item);
    return shape && shape->apexAngleHandle() && !shape->isCustomShape();
}

qreal CanvasItemHandles::size() const
{
    return qBound<qreal>(6.0, 9.0 / m_item->viewScale(), 40.0);
}

qreal CanvasItemHandles::rotationOffset() const
{
    // Keep the rotation handle one handle-size plus a small gap above the frame,
    // so it stays visually separate from the Top resize handle at any zoom.
    return size() + 16.0 / m_item->viewScale();
}

QPointF CanvasItemHandles::center(int handle) const
{
    const QRectF rect = m_item->frameRect();
    switch (static_cast<CanvasItem::Handle>(handle)) {
    case CanvasItem::TopLeft:
        return rect.topLeft();
    case CanvasItem::Top:
        return QPointF(rect.center().x(), rect.top());
    case CanvasItem::TopRight:
        return rect.topRight();
    case CanvasItem::Right:
        return QPointF(rect.right(), rect.center().y());
    case CanvasItem::BottomRight:
        return rect.bottomRight();
    case CanvasItem::Bottom:
        return QPointF(rect.center().x(), rect.bottom());
    case CanvasItem::BottomLeft:
        return rect.bottomLeft();
    case CanvasItem::Left:
        return QPointF(rect.left(), rect.center().y());
    case CanvasItem::Rotation:
        return QPointF(rect.center().x(), rect.top() - rotationOffset());
    case CanvasItem::Apex: {
        bool ok = false;
        const QPointF custom = m_item->customHandleCenter(CanvasItem::Apex, &ok);
        if (ok) {
            return custom;
        }
        break;
    }
    case CanvasItem::NoHandle:
        break;
    }
    return rect.center();
}

QRectF CanvasItemHandles::rect(int handle) const
{
    const qreal handleSize = size();
    const QPointF handleCenter = center(handle);
    return QRectF(handleCenter.x() - handleSize / 2.0, handleCenter.y() - handleSize / 2.0, handleSize, handleSize);
}

int CanvasItemHandles::hitAt(const QPointF& pos) const
{
    const qreal hitPadding = qMin<qreal>(10.0 / m_item->viewScale(), 14.0);
    for (int handle = CanvasItem::TopLeft; handle <= CanvasItem::Rotation; ++handle) {
        if (handle == CanvasItem::Top && usesApexHandle()) {
            continue;
        }
        const QRectF handleRect = rect(handle).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (handleRect.contains(pos)) {
            return handle;
        }
    }
    if (usesApexHandle()) {
        const QRectF handleRect = rect(CanvasItem::Apex).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (handleRect.contains(pos)) {
            return CanvasItem::Apex;
        }
    }
    return CanvasItem::NoHandle;
}

QPainterPath CanvasItemHandles::selectionShape() const
{
    QPainterPath path;
    if (!m_item->isSelected() || !m_item->isEditable() || !m_item->hasSingleSelection()) {
        return path;
    }

    // Selection handles are centred on the content corners, so half of every
    // handle lies outside the item geometry. Drawing them without adding them
    // to shape() leaves the visible handle unclickable; for outlines that never
    // reach the corner (ellipse, triangle, diamond, star) the handle could not
    // be grabbed at all.
    for (int handle = CanvasItem::TopLeft; handle <= CanvasItem::Rotation; ++handle) {
        if (handle == CanvasItem::Top && usesApexHandle()) {
            continue;
        }
        path.addRect(
            rect(handle).adjusted(-kHandleHitPadding, -kHandleHitPadding, kHandleHitPadding, kHandleHitPadding));
    }
    if (usesApexHandle()) {
        path.addRect(rect(CanvasItem::Apex)
                         .adjusted(-kHandleHitPadding, -kHandleHitPadding, kHandleHitPadding, kHandleHitPadding));
    }
    return path;
}

void CanvasItemHandles::drawOverlay(QPainter* painter) const
{
    QPen borderPen(QColor(79, 124, 255));
    borderPen.setWidthF(1.3);
    borderPen.setCosmetic(true);
    borderPen.setStyle(Qt::DashLine);
    borderPen.setDashPattern({5.0, 4.0});
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_item->frameRect());

    if (!m_item->isEditable() || !m_item->hasSingleSelection()) {
        return;
    }

    QPen handlePen(QColor(79, 124, 255));
    handlePen.setWidthF(1.4);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int handle = CanvasItem::TopLeft; handle <= CanvasItem::Left; ++handle) {
        if (handle == CanvasItem::Top && usesApexHandle()) {
            continue;
        }
        painter->drawRect(rect(handle));
    }
    if (usesApexHandle()) {
        painter->drawRect(rect(CanvasItem::Apex));
    }

    // Rotation handle: a circular knob above the frame, connected by a thin line.
    const QPointF topCenter(center(CanvasItem::Top).x(), m_item->frameRect().top());
    const QPointF rotationCenter = center(CanvasItem::Rotation);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(QColor(79, 124, 255));
    painter->drawEllipse(rect(CanvasItem::Rotation).adjusted(1.0, 1.0, -1.0, -1.0));
}

QCursor CanvasItemHandles::cursor(int handle) const
{
    switch (static_cast<CanvasItem::Handle>(handle)) {
    case CanvasItem::TopLeft:
    case CanvasItem::BottomRight:
        return QCursor(Qt::SizeFDiagCursor);
    case CanvasItem::TopRight:
    case CanvasItem::BottomLeft:
        return QCursor(Qt::SizeBDiagCursor);
    case CanvasItem::Top:
    case CanvasItem::Bottom:
        return QCursor(Qt::SizeVerCursor);
    case CanvasItem::Left:
    case CanvasItem::Right:
        return QCursor(Qt::SizeHorCursor);
    case CanvasItem::Apex:
        return QCursor(Qt::SizeAllCursor);
    case CanvasItem::Rotation:
        return QCursor(Qt::PointingHandCursor);
    case CanvasItem::NoHandle:
        break;
    }
    return QCursor(Qt::ArrowCursor);
}
