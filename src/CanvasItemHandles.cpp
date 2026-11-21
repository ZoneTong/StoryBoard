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
        return QPointF(rect.center().x(), rect.top() - rotationOffset());
    case CanvasItem::APEX: {
        bool ok = false;
        const QPointF custom = m_item->customHandleCenter(CanvasItem::APEX, &ok);
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

QRectF CanvasItemHandles::rect(int handle) const
{
    const qreal handleSize = size();
    const QPointF handleCenter = center(handle);
    return QRectF(handleCenter.x() - handleSize / 2.0, handleCenter.y() - handleSize / 2.0, handleSize, handleSize);
}

int CanvasItemHandles::hitAt(const QPointF& pos) const
{
    const qreal hitPadding = qMin<qreal>(10.0 / m_item->viewScale(), 14.0);
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && usesApexHandle()) {
            continue;
        }
        const QRectF handleRect = rect(handle).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (handleRect.contains(pos)) {
            return handle;
        }
    }
    if (usesApexHandle()) {
        const QRectF handleRect = rect(CanvasItem::APEX).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (handleRect.contains(pos)) {
            return CanvasItem::APEX;
        }
    }
    return CanvasItem::NO_HANDLE;
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
    // reach the corner (ellipse, triangle, parallelogram, star) the handle could not
    // be grabbed at all.
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && usesApexHandle()) {
            continue;
        }
        path.addRect(
            rect(handle).adjusted(-HANDLE_HIT_PADDING, -HANDLE_HIT_PADDING, HANDLE_HIT_PADDING, HANDLE_HIT_PADDING));
    }
    if (usesApexHandle()) {
        path.addRect(rect(CanvasItem::APEX)
                         .adjusted(-HANDLE_HIT_PADDING, -HANDLE_HIT_PADDING, HANDLE_HIT_PADDING, HANDLE_HIT_PADDING));
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
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::LEFT; ++handle) {
        if (handle == CanvasItem::TOP && usesApexHandle()) {
            continue;
        }
        painter->drawRect(rect(handle));
    }
    if (usesApexHandle()) {
        painter->drawRect(rect(CanvasItem::APEX));
    }

    // Rotation handle: a circular knob above the frame, connected by a thin line.
    const QPointF topCenter(center(CanvasItem::TOP).x(), m_item->frameRect().top());
    const QPointF rotationCenter = center(CanvasItem::ROTATION);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(QColor(79, 124, 255));
    painter->drawEllipse(rect(CanvasItem::ROTATION).adjusted(1.0, 1.0, -1.0, -1.0));
}

QCursor CanvasItemHandles::cursor(int handle) const
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
