#include "canvas/controllers/CanvasItemHandles.h"

#include "canvas/SelectionOverlayStyle.h"
#include "items/CanvasItem.h"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>

namespace {
// 内角旋钮：白底小方块里画一个水平双箭头，暗示“左右拖，内角连续变”。
// 三角形顶点手柄与平行四边形倾斜旋钮共用这一份样式（P2 用户口径）。
void DrawAngleKnob(QPainter* painter, const QRectF& knob)
{
    painter->setBrush(Qt::white);
    painter->drawRect(knob);
    painter->setBrush(SelectionOverlayStyle::HandleColor());
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
    return SelectionOverlayStyle::KnobSize(m_item->ViewScale());
}

qreal CanvasItemHandles::RotationOffset() const
{
    // 与组框同一份口径：一个手柄再加一点间隙，任何缩放下都不会和顶边手柄挤在一起。
    return SelectionOverlayStyle::RotationOffset(m_item->ViewScale());
}

QPointF CanvasItemHandles::Center(int handle) const
{
    HandleSpec spec;
    if (SpecAt(handle, &spec)) {
        // 镜像只改显示矩阵，自定义手柄的本地位置要同步过一遍镜像，才落在看得见的端点上。
        if (m_item->FlipX() || m_item->FlipY()) {
            return m_item->MirrorTransform().map(spec.position);
        }
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
        return SelectionOverlayStyle::RotationKnobCenter(rect, m_item->ViewScale());
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
    return SelectionOverlayStyle::KnobRect(Center(handle), handleSize);
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

bool CanvasItemHandles::IsAngleKnob(const HandleSpec& spec)
{
    return spec.behavior == HandleBehavior::ANGLE || spec.behavior == HandleBehavior::SKEW;
}

bool CanvasItemHandles::HitAngleKnob(const HandleSpec& spec, const QPointF& pos) const
{
    // 角度旋钮按“到旋钮中心的距离”判定：圆心必然命中，缩放到 25% / 400% 时
    // 方块命中区跟着变大变小，圆心始终在区内，不再出现“点到旋钮中心却拖成缩放”。
    const qreal radius = HitRect(spec.id).width() / 2.0;
    return QLineF(spec.position, pos).length() <= radius;
}

int CanvasItemHandles::HitAt(const QPointF& pos) const
{
    const HandleSpecList specs = ActiveSpecs();
    auto hitSpec = [this, &pos](const HandleSpec& spec) {
        return IsAngleKnob(spec) ? HitAngleKnob(spec, pos) : HitRect(spec.id).contains(pos);
    };

    // 角度旋钮（三角形 / 平行四边形 / 菱形的内角）按到中心的距离取最近的一个；
    // onRect 单独记“指针正压在旋钮方块上”，用来和角上的缩放手柄分胜负。
    int knobHit = CanvasItem::NO_HANDLE;
    qreal knobDistance = 0.0;
    bool knobOnRect = false;
    for (const HandleSpec& spec : specs) {
        if (!IsAngleKnob(spec) || !hitSpec(spec)) {
            continue;
        }
        const qreal distance = QLineF(spec.position, pos).length();
        if (knobHit == CanvasItem::NO_HANDLE || distance < knobDistance) {
            knobHit = spec.id;
            knobDistance = distance;
        }
        knobOnRect = knobOnRect || Rect(spec.id).contains(pos);
    }

    // 选择框上的手柄同样取最近的一个（重合时中心距离才是可比较的判据）。
    int handleHit = CanvasItem::NO_HANDLE;
    qreal handleDistance = 0.0;
    bool handleOnRect = false;
    auto considerHandle = [this, &pos, &handleHit, &handleDistance, &handleOnRect](int handle) {
        const qreal distance = QLineF(Center(handle), pos).length();
        if (handleHit == CanvasItem::NO_HANDLE || distance < handleDistance) {
            handleHit = handle;
            handleDistance = distance;
        }
        handleOnRect = handleOnRect || Rect(handle).contains(pos);
    };
    bool hidesTop = false;
    for (const HandleSpec& spec : specs) {
        if (spec.replacesTopHandle) {
            hidesTop = true;
        } else if (!IsAngleKnob(spec) && hitSpec(spec)) {
            considerHandle(spec.id);
        }
    }
    for (int handle = CanvasItem::TOP_LEFT; handle <= CanvasItem::ROTATION; ++handle) {
        if (handle == CanvasItem::TOP && hidesTop) {
            continue;
        }
        if (HitRect(handle).contains(pos)) {
            considerHandle(handle);
        }
    }

    // 内角旋钮就贴在角的顶点内侧，和角上的缩放手柄只隔几像素：指针压在哪一个的方块上，
    // 就归哪一方；只有都落在对方外扩的命中区里时，才回到“到各自中心的距离”比远近。
    // 这样既不丢缩放手柄，也不会让用户“明明点在旋钮上却拖成了缩放”。
    if (knobHit != CanvasItem::NO_HANDLE) {
        if (knobOnRect != handleOnRect) {
            return knobOnRect ? knobHit : handleHit;
        }
        if (handleHit == CanvasItem::NO_HANDLE || knobDistance <= handleDistance) {
            return knobHit;
        }
    }
    return handleHit;
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
    painter->setPen(SelectionOverlayStyle::BorderPen());
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_item->FrameRect());

    if (!m_item->IsEditable() || !m_item->HasSingleSelection()) {
        return;
    }

    painter->setPen(SelectionOverlayStyle::KnobPen());
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
    painter->setBrush(SelectionOverlayStyle::HandleColor());
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
