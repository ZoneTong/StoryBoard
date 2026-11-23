#include "canvas/controllers/CanvasItemHandles.h"

#include "items/CanvasItem.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal KNOB_STROKE = 1.4;
const QColor HANDLE_COLOR(79, 124, 255);

// 内角旋钮：白底小方块里画一个水平双箭头，暗示“左右拖，内角连续变”。
// 三角形顶点手柄与平行四边形倾斜旋钮共用这一份样式（P2 用户口径）。
void DrawAngleKnob(QPainter* painter, const QRectF& knob)
{
    painter->setBrush(Qt::white);
    painter->drawRect(knob);
    painter->setBrush(HANDLE_COLOR);
    const QPointF center = knob.center();
    const qreal halfLength = knob.width() * 0.26;
    const qreal head = knob.width() * 0.14;
    const QPointF left = center - QPointF(halfLength, 0.0);
    const QPointF right = center + QPointF(halfLength, 0.0);
    painter->drawLine(left, right);
    // 两端的箭头用线段画，避免 QPainterPath 的坐标重载歧义。
    painter->drawLine(left, left + QPointF(head, -head * 0.8));
    painter->drawLine(left, left + QPointF(head, head * 0.8));
    painter->drawLine(right, right - QPointF(head, -head * 0.8));
    painter->drawLine(right, right - QPointF(head, head * 0.8));
}
} // namespace

CanvasItemHandles::CanvasItemHandles(CanvasItem* item) : m_item(item) {}

HandleSpecList CanvasItemHandles::ActiveSpecs() const
{
    if (!m_item || !m_item->isSelected() || !m_item->IsEditable() || !m_item->HasSingleSelection()) {
        return HandleSpecList();
    }
    return m_item->CustomHandles();
}

bool CanvasItemHandles::SpecAt(int handle, HandleSpec* spec) const
{
    if (handle < CanvasItem::CUSTOM_0 || !spec) {
        return false;
    }
    for (const HandleSpec& candidate : ActiveSpecs()) {
        if (candidate.id == handle) {
            *spec = candidate;
            return true;
        }
    }
    return false;
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
    HandleSpec spec;
    if (SpecAt(handle, &spec)) {
        return spec.position;
    }
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
    case CanvasItem::CUSTOM_0:
    case CanvasItem::CUSTOM_1:
    case CanvasItem::NO_HANDLE:
        break;
    }
    return rect.center();
}

QRectF CanvasItemHandles::Rect(int handle) const
{
    HandleSpec spec;
    const bool custom = SpecAt(handle, &spec);
    const qreal handleSize = Size() * (custom ? qMax<qreal>(0.6, spec.sizeScale) : 1.0);
    const QPointF handleCenter = Center(handle);
    return QRectF(handleCenter.x() - handleSize / 2.0, handleCenter.y() - handleSize / 2.0, handleSize, handleSize);
}

qreal CanvasItemHandles::HitReach() const
{
    qreal padding = qMin<qreal>(10.0 / m_item->ViewScale(), 14.0);
    for (const HandleSpec& spec : ActiveSpecs()) {
        padding = qMax(padding, spec.hitPadding / m_item->ViewScale());
    }
    return Size() / 2.0 + padding;
}

QRectF CanvasItemHandles::HitRect(int handle) const
{
    HandleSpec spec;
    const bool custom = SpecAt(handle, &spec);
    qreal padding = qMin<qreal>(10.0 / m_item->ViewScale(), 14.0);
    if (custom) {
        padding = qMax(padding, spec.hitPadding / m_item->ViewScale());
    }
    return Rect(handle).adjusted(-padding, -padding, padding, padding);
}

int CanvasItemHandles::HitAt(const QPointF& pos) const
{
    const HandleSpecList specs = ActiveSpecs();
    auto hitSpec = [this, &pos](const HandleSpec& spec) { return HitRect(spec.id).contains(pos); };
    bool hidesTop = false;
    for (const HandleSpec& spec : specs) {
        if (spec.replacesTopHandle) {
            hidesTop = true;
            continue;
        }
        // 与顶边冲突的自定义句柄（平行四边形旋钮）优先于通用手柄，避免命中区重叠。
        if (hitSpec(spec)) {
            return spec.id;
        }
    }
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && hidesTop) {
            continue;
        }
        if (HitRect(handle).contains(pos)) {
            return handle;
        }
    }
    // 顶边上的自定义句柄（三角形顶点 C）放在最后，让角落手柄先赢。
    for (const HandleSpec& spec : specs) {
        if (spec.replacesTopHandle && hitSpec(spec)) {
            return spec.id;
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
    const HandleSpecList specs = ActiveSpecs();
    bool hidesTop = false;
    for (const HandleSpec& spec : specs) {
        hidesTop = hidesTop || spec.replacesTopHandle;
    }
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && hidesTop) {
            continue;
        }
        path.addRect(HitRect(handle));
    }
    for (const HandleSpec& spec : specs) {
        path.addRect(HitRect(spec.id));
    }
    return path;
}

void CanvasItemHandles::DrawOverlay(QPainter* painter) const
{
    QPen borderPen(HANDLE_COLOR);
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

    QPen handlePen(HANDLE_COLOR);
    handlePen.setWidthF(KNOB_STROKE);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    const HandleSpecList specs = ActiveSpecs();
    bool hidesTop = false;
    for (const HandleSpec& spec : specs) {
        hidesTop = hidesTop || spec.replacesTopHandle;
    }
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::LEFT; ++handle) {
        if (handle == CanvasItem::TOP && hidesTop) {
            continue;
        }
        painter->drawRect(Rect(handle));
    }
    // 自定义句柄：调角度的两类（三角顶点 C、平行四边形旋钮）共用双箭头旋钮样式，
    // 其余（弧线 / 扇形端点）画成普通白底方块。
    for (const HandleSpec& spec : specs) {
        const QRectF knob = Rect(spec.id);
        if (spec.behavior == HandleBehavior::ANGLE || spec.behavior == HandleBehavior::SKEW) {
            DrawAngleKnob(painter, knob);
        } else {
            painter->setBrush(Qt::white);
            painter->drawRect(knob);
        }
    }

    // Rotation handle: a circular knob above the frame, connected by a thin line.
    const QPointF topCenter(Center(CanvasItem::TOP).x(), m_item->FrameRect().top());
    const QPointF rotationCenter = Center(CanvasItem::ROTATION);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(HANDLE_COLOR);
    painter->drawEllipse(Rect(CanvasItem::ROTATION).adjusted(1.0, 1.0, -1.0, -1.0));
}

QCursor CanvasItemHandles::Cursor(int handle) const
{
    HandleSpec spec;
    if (SpecAt(handle, &spec)) {
        return QCursor(spec.cursor);
    }
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
    case CanvasItem::ROTATION:
        return QCursor(Qt::PointingHandCursor);
    case CanvasItem::CUSTOM_0:
    case CanvasItem::CUSTOM_1:
    case CanvasItem::NO_HANDLE:
        break;
    }
    return QCursor(Qt::ArrowCursor);
}
