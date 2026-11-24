#include "canvas/controllers/CanvasGroupBoxController.h"

#include "canvas/CanvasView.h"
#include "canvas/SelectionOverlayStyle.h"
#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"

#include <QCoreApplication>
#include <QPainter>
#include <QtMath>
#include <cmath>
#include <utility>

namespace {

constexpr int RESIZE_HANDLES_BEGIN = static_cast<int>(GroupBoxHandle::NW);
constexpr int RESIZE_HANDLES_END = static_cast<int>(GroupBoxHandle::W);
constexpr qreal HANDLE_PADDING = 10.0;

bool IsEdgeHandle(int handle)
{
    return handle == static_cast<int>(GroupBoxHandle::N) || handle == static_cast<int>(GroupBoxHandle::E) ||
           handle == static_cast<int>(GroupBoxHandle::S) || handle == static_cast<int>(GroupBoxHandle::W);
}

bool IsCornerHandle(int handle)
{
    return handle == static_cast<int>(GroupBoxHandle::NW) || handle == static_cast<int>(GroupBoxHandle::NE) ||
           handle == static_cast<int>(GroupBoxHandle::SE) || handle == static_cast<int>(GroupBoxHandle::SW);
}

} // namespace

CanvasGroupBoxController::CanvasGroupBoxController(CanvasView* view, CanvasScene* scene) : m_view(view), m_scene(scene)
{
}

bool CanvasGroupBoxController::Active() const
{
    return GroupId() > 0;
}

int CanvasGroupBoxController::GroupId() const
{
    int groupId = 0;
    if (m_scene) {
        m_scene->SelectionIsCompleteGroup(&groupId);
    }
    return groupId;
}

QRectF CanvasGroupBoxController::GroupRect() const
{
    const int groupId = GroupId();
    return groupId > 0 ? m_scene->GroupBounds(groupId) : QRectF();
}

QRectF CanvasGroupBoxController::ViewportRect() const
{
    if (!m_view) {
        return QRectF();
    }
    return m_view->mapFromScene(GroupRect()).boundingRect();
}

bool CanvasGroupBoxController::Rotating() const
{
    return m_dragging && m_handle == static_cast<int>(GroupBoxHandle::ROTATE);
}

QRectF CanvasGroupBoxController::OverlayRect() const
{
    // 旋转期间画的是“起手外框”跟着刚转：成员绕组中心刚体旋转后，
    // 轴对齐包围盒会跟着变宽变窄，若拿实时包围盒当选框，框会相对成员漂。
    if (Rotating() && m_view && !m_startRect.isNull()) {
        return m_view->mapFromScene(m_startRect).boundingRect();
    }
    return ViewportRect();
}

QTransform CanvasGroupBoxController::OverlayRotation() const
{
    // 只有“正在拖旋转钮”才让选框跟着转：起转前与缩放拖动时都是恒等变换，
    // 命中 / 光标沿用轴对齐的组框。
    if (!Rotating() || qFuzzyIsNull(m_dragAngle) || !m_view) {
        return QTransform();
    }
    const QPointF center = m_view->mapFromScene(m_rotateCenter);
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotateRadians(m_dragAngle);
    transform.translate(-center.x(), -center.y());
    return transform;
}

QList<CanvasItem*> CanvasGroupBoxController::Members() const
{
    const int groupId = GroupId();
    return groupId > 0 ? m_scene->AllLeafItems(groupId) : QList<CanvasItem*>();
}

QList<CanvasGroupBoxController::MemberSnapshot> CanvasGroupBoxController::Snapshot() const
{
    QList<MemberSnapshot> snapshot;
    for (CanvasItem* item : Members()) {
        if (!item) {
            continue;
        }
        MemberSnapshot entry;
        entry.item = item;
        entry.oldPos = item->pos();
        entry.oldSize = item->ContentSize();
        entry.oldRotation = item->RotationAngle();
        entry.oldCenter = item->mapToScene(item->ContentRect().center());
        snapshot.append(entry);
    }
    return snapshot;
}

QPointF CanvasGroupBoxController::HandleCenter(int handle, const QRectF& rect) const
{
    switch (handle) {
    case static_cast<int>(GroupBoxHandle::NW):
        return rect.topLeft();
    case static_cast<int>(GroupBoxHandle::N):
        return QPointF(rect.center().x(), rect.top());
    case static_cast<int>(GroupBoxHandle::NE):
        return rect.topRight();
    case static_cast<int>(GroupBoxHandle::E):
        return QPointF(rect.right(), rect.center().y());
    case static_cast<int>(GroupBoxHandle::SE):
        return rect.bottomRight();
    case static_cast<int>(GroupBoxHandle::S):
        return QPointF(rect.center().x(), rect.bottom());
    case static_cast<int>(GroupBoxHandle::SW):
        return rect.bottomLeft();
    case static_cast<int>(GroupBoxHandle::W):
        return QPointF(rect.left(), rect.center().y());
    case static_cast<int>(GroupBoxHandle::ROTATE):
        // 视口坐标里缩放恒为 1，和单图元取同一份手柄尺寸与间隙。
        return SelectionOverlayStyle::RotationKnobCenter(rect, 1.0);
    default:
        return rect.center();
    }
}

QRectF CanvasGroupBoxController::ResizeRect(int handle, const QRectF& startRect, const QPointF& scenePos) const
{
    // 组框只做等比缩放：只有角手柄，两轴取同一个倍率（符号各自保留以支持翻转）。
    if (!IsCornerHandle(handle) || startRect.width() <= 0.0 || startRect.height() <= 0.0) {
        return startRect;
    }
    const qreal rawX = (scenePos.x() - startRect.left()) / startRect.width();
    const qreal rawY = (scenePos.y() - startRect.top()) / startRect.height();
    qreal scaleX = handle == static_cast<int>(GroupBoxHandle::NW) || handle == static_cast<int>(GroupBoxHandle::SW)
                       ? 1.0 - rawX
                       : rawX;
    qreal scaleY = handle == static_cast<int>(GroupBoxHandle::NW) || handle == static_cast<int>(GroupBoxHandle::NE)
                       ? 1.0 - rawY
                       : rawY;
    const qreal magnitude = qMax(std::fabs(scaleX), std::fabs(scaleY));
    scaleX = scaleX < 0.0 ? -magnitude : magnitude;
    scaleY = scaleY < 0.0 ? -magnitude : magnitude;

    QRectF result = startRect;
    switch (handle) {
    case static_cast<int>(GroupBoxHandle::NW):
        result.setLeft(startRect.left() + startRect.width() * (1.0 - scaleX));
        result.setTop(startRect.top() + startRect.height() * (1.0 - scaleY));
        break;
    case static_cast<int>(GroupBoxHandle::NE):
        result.setRight(startRect.left() + startRect.width() * scaleX);
        result.setTop(startRect.top() + startRect.height() * (1.0 - scaleY));
        break;
    case static_cast<int>(GroupBoxHandle::SE):
        result.setRight(startRect.left() + startRect.width() * scaleX);
        result.setBottom(startRect.top() + startRect.height() * scaleY);
        break;
    case static_cast<int>(GroupBoxHandle::SW):
        result.setLeft(startRect.left() + startRect.width() * (1.0 - scaleX));
        result.setBottom(startRect.top() + startRect.height() * scaleY);
        break;
    default:
        break;
    }
    result = result.normalized();
    const QSizeF minimum = CanvasItem::MinimumSize();
    if (result.width() < minimum.width()) {
        result.setWidth(minimum.width());
    }
    if (result.height() < minimum.height()) {
        result.setHeight(minimum.height());
    }
    return result;
}

QPointF CanvasGroupBoxController::RotateAround(const QPointF& point, const QPointF& center, qreal angle)
{
    const qreal dx = point.x() - center.x();
    const qreal dy = point.y() - center.y();
    const qreal cosValue = std::cos(angle);
    const qreal sinValue = std::sin(angle);
    return QPointF(center.x() + dx * cosValue - dy * sinValue, center.y() + dx * sinValue + dy * cosValue);
}

int CanvasGroupBoxController::HitTest(const QPoint& viewportPos) const
{
    if (!Active() || !m_view) {
        return static_cast<int>(GroupBoxHandle::NONE);
    }
    const QRectF rect = OverlayRect();
    if (rect.isNull()) {
        return static_cast<int>(GroupBoxHandle::NONE);
    }
    // 旋转拖动中选框已经转了角度：先把鼠标点反向转回轴对齐坐标再判命中。
    const QPointF point = OverlayRotation().inverted().map(QPointF(viewportPos));
    const qreal knob = SelectionOverlayStyle::KnobSize(1.0);

    const QRectF rotateKnob =
        SelectionOverlayStyle::KnobRect(HandleCenter(static_cast<int>(GroupBoxHandle::ROTATE), rect), knob);
    if (rotateKnob.adjusted(-HANDLE_PADDING, -HANDLE_PADDING, HANDLE_PADDING, HANDLE_PADDING).contains(point)) {
        return static_cast<int>(GroupBoxHandle::ROTATE);
    }

    // 组框只做等比缩放：只保留四角手柄，边手柄既不画也不参与命中。
    for (int handle = RESIZE_HANDLES_BEGIN; handle <= RESIZE_HANDLES_END; ++handle) {
        if (IsEdgeHandle(handle)) {
            continue;
        }
        const QRectF hit = SelectionOverlayStyle::KnobRect(HandleCenter(handle, rect), knob);
        if (hit.adjusted(-HANDLE_PADDING, -HANDLE_PADDING, HANDLE_PADDING, HANDLE_PADDING).contains(point)) {
            return handle;
        }
    }

    if (rect.contains(point)) {
        return static_cast<int>(GroupBoxHandle::BODY);
    }
    return static_cast<int>(GroupBoxHandle::NONE);
}

void CanvasGroupBoxController::BeginDrag(const QPoint& viewportPos, int handle)
{
    if (!Active() || handle == static_cast<int>(GroupBoxHandle::NONE)) {
        return;
    }
    m_startRect = GroupRect();
    m_snapshot = Snapshot();
    m_handle = handle;
    m_dragging = true;
    m_dragAngle = 0.0;

    if (handle == static_cast<int>(GroupBoxHandle::ROTATE)) {
        m_rotateCenter = m_startRect.center();
        const QPointF point = m_view->mapToScene(viewportPos);
        m_startAngle = std::atan2(point.y() - m_rotateCenter.y(), point.x() - m_rotateCenter.x());
    }
}

void CanvasGroupBoxController::UpdateDrag(const QPoint& viewportPos, Qt::KeyboardModifiers modifiers)
{
    if (!m_dragging || !m_view) {
        return;
    }
    const QPointF scenePos = m_view->mapToScene(viewportPos);

    if (m_handle == static_cast<int>(GroupBoxHandle::ROTATE)) {
        const qreal angle =
            std::atan2(scenePos.y() - m_rotateCenter.y(), scenePos.x() - m_rotateCenter.x()) - m_startAngle;
        m_dragAngle = angle;
        // m_dragAngle 只管选框叠层（QTransform::rotateRadians）；成员角度走 SetRotationAngle，
        // 那一侧是角度制（与单图元旋转手柄一致），这里必须换算，否则成员只挪中心不自转。
        const qreal angleDegrees = qRadiansToDegrees(angle);
        for (const MemberSnapshot& entry : std::as_const(m_snapshot)) {
            if (!entry.item) {
                continue;
            }
            // 每一步都先把成员退回起手快照，再按“累计角度”重算一次刚体变换。
            // 若直接在上一帧的结果上继续转，指针每动一下都会再叠加一次累计角，
            // 成员的圆心就会各走各的、内部相对位置被拉开（旧实现的漂移就出在这里）。
            entry.item->setPos(entry.oldPos);
            entry.item->SetContentSize(entry.oldSize);
            entry.item->SetRotationAngle(CanvasItem::NormalizeRotation(entry.oldRotation + angleDegrees));
            // 刚体变换：成员起手中心绕组中心转 angle，再按同一角度自转。
            const QPointF localCenter = entry.item->ContentRect().center();
            const QPointF target = RotateAround(entry.oldCenter, m_rotateCenter, angle);
            // 自转本身可能挪动中心（旋转原点与内容中心不一致时），按差值把中心搬回目标点。
            entry.item->setPos(entry.item->pos() + (target - entry.item->mapToScene(localCenter)));
        }
        // 选框与旋转手柄也要绕组中心转起来，跟单图元一致。
        if (m_view) {
            m_view->viewport()->update();
        }
        return;
    }

    // 组框只做等比缩放：角手柄恒按两轴同倍率走，Shift 不再改变行为。
    Q_UNUSED(modifiers)
    const QRectF newRect = ResizeRect(m_handle, m_startRect, scenePos);
    const qreal scaleX = m_startRect.width() > 0.0 ? newRect.width() / m_startRect.width() : 1.0;
    const qreal scaleY = m_startRect.height() > 0.0 ? newRect.height() / m_startRect.height() : 1.0;
    const QSizeF minimum = CanvasItem::MinimumSize();

    for (const MemberSnapshot& entry : std::as_const(m_snapshot)) {
        if (!entry.item) {
            continue;
        }
        const QPointF relative = entry.oldPos - m_startRect.topLeft();
        const QPointF newPos(newRect.left() + relative.x() * scaleX, newRect.top() + relative.y() * scaleY);
        QSizeF newSize(entry.oldSize.width() * scaleX, entry.oldSize.height() * scaleY);
        newSize.setWidth(qMax(minimum.width(), newSize.width()));
        newSize.setHeight(qMax(minimum.height(), newSize.height()));
        entry.item->setPos(newPos);
        entry.item->SetContentSize(newSize);
    }
}

void CanvasGroupBoxController::EndDrag()
{
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    m_dragAngle = 0.0;
    if (m_snapshot.isEmpty() || !m_scene) {
        return;
    }

    if (m_handle == static_cast<int>(GroupBoxHandle::ROTATE)) {
        QList<CanvasItem*> items;
        QList<qreal> anglesBefore;
        QList<qreal> anglesAfter;
        ItemPlacementList geometryBefore;
        ItemPlacementList geometryAfter;
        for (const MemberSnapshot& entry : std::as_const(m_snapshot)) {
            if (!entry.item) {
                continue;
            }
            items.append(entry.item);
            anglesBefore.append(entry.oldRotation);
            anglesAfter.append(entry.item->RotationAngle());
            geometryBefore.append({entry.item, entry.oldPos, entry.oldSize});
            geometryAfter.append({entry.item, entry.item->pos(), entry.item->ContentSize()});
        }
        m_scene->PushGroupRotation(items, anglesBefore, anglesAfter, geometryBefore, geometryAfter,
                                   QCoreApplication::tr("Rotate group"));
        return;
    }

    ItemPlacementList geometryBefore;
    ItemPlacementList geometryAfter;
    for (const MemberSnapshot& entry : std::as_const(m_snapshot)) {
        if (!entry.item) {
            continue;
        }
        geometryBefore.append({entry.item, entry.oldPos, entry.oldSize});
        geometryAfter.append({entry.item, entry.item->pos(), entry.item->ContentSize()});
    }
    m_scene->PushGeometry(geometryBefore, geometryAfter, QCoreApplication::tr("Resize group"));
}

void CanvasGroupBoxController::DrawOverlay(QPainter* painter) const
{
    if (!Active() || !painter) {
        return;
    }
    const QRectF rect = OverlayRect();
    if (rect.isNull()) {
        return;
    }

    // 旋转拖动期间选框、手柄与旋转钮一起绕组中心转，行为跟单图元一致。
    painter->save();
    painter->setTransform(OverlayRotation(), true);

    painter->setPen(SelectionOverlayStyle::BorderPen());
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    const qreal knob = SelectionOverlayStyle::KnobSize(1.0);
    painter->setPen(SelectionOverlayStyle::KnobPen());
    painter->setBrush(Qt::white);
    for (int handle = RESIZE_HANDLES_BEGIN; handle <= RESIZE_HANDLES_END; ++handle) {
        if (IsEdgeHandle(handle)) {
            continue;
        }
        painter->drawRect(SelectionOverlayStyle::KnobRect(HandleCenter(handle, rect), knob));
    }

    const QPointF topCenter(rect.center().x(), rect.top());
    const QPointF rotationCenter = HandleCenter(static_cast<int>(GroupBoxHandle::ROTATE), rect);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(SelectionOverlayStyle::HandleColor());
    painter->drawEllipse(SelectionOverlayStyle::KnobRect(rotationCenter, knob).adjusted(1.0, 1.0, -1.0, -1.0));
    painter->restore();
}
