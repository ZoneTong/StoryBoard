#include "CanvasGroupBoxController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"

#include <QCoreApplication>
#include <QPainter>
#include <QtMath>
#include <cmath>
#include <utility>

namespace {

constexpr int RESIZE_HANDLES_BEGIN = static_cast<int>(GroupBoxHandle::NW);
constexpr int RESIZE_HANDLES_END = static_cast<int>(GroupBoxHandle::W);
constexpr qreal HANDLE_SIZE = 9.0;
constexpr qreal HANDLE_PADDING = 10.0;

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
        return QPointF(rect.center().x(), rect.top() - (HANDLE_SIZE + 16.0));
    default:
        return rect.center();
    }
}

QRectF CanvasGroupBoxController::ResizeRect(int handle, const QRectF& startRect, const QPointF& scenePos) const
{
    QRectF result = startRect;
    switch (handle) {
    case static_cast<int>(GroupBoxHandle::NW):
        result.setLeft(scenePos.x());
        result.setTop(scenePos.y());
        break;
    case static_cast<int>(GroupBoxHandle::N):
        result.setTop(scenePos.y());
        break;
    case static_cast<int>(GroupBoxHandle::NE):
        result.setRight(scenePos.x());
        result.setTop(scenePos.y());
        break;
    case static_cast<int>(GroupBoxHandle::E):
        result.setRight(scenePos.x());
        break;
    case static_cast<int>(GroupBoxHandle::SE):
        result.setRight(scenePos.x());
        result.setBottom(scenePos.y());
        break;
    case static_cast<int>(GroupBoxHandle::S):
        result.setBottom(scenePos.y());
        break;
    case static_cast<int>(GroupBoxHandle::SW):
        result.setLeft(scenePos.x());
        result.setBottom(scenePos.y());
        break;
    case static_cast<int>(GroupBoxHandle::W):
        result.setLeft(scenePos.x());
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
    const QRectF rect = ViewportRect();
    if (rect.isNull()) {
        return static_cast<int>(GroupBoxHandle::NONE);
    }

    const QRectF rotateKnob(HandleCenter(static_cast<int>(GroupBoxHandle::ROTATE), rect),
                            QSizeF(HANDLE_SIZE, HANDLE_SIZE));
    if (rotateKnob.adjusted(-HANDLE_PADDING, -HANDLE_PADDING, HANDLE_PADDING, HANDLE_PADDING).contains(viewportPos)) {
        return static_cast<int>(GroupBoxHandle::ROTATE);
    }

    for (int handle = RESIZE_HANDLES_BEGIN; handle <= RESIZE_HANDLES_END; ++handle) {
        const QRectF knob(HandleCenter(handle, rect), QSizeF(HANDLE_SIZE, HANDLE_SIZE));
        if (knob.adjusted(-HANDLE_PADDING, -HANDLE_PADDING, HANDLE_PADDING, HANDLE_PADDING).contains(viewportPos)) {
            return handle;
        }
    }

    if (rect.contains(viewportPos)) {
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

    if (handle == static_cast<int>(GroupBoxHandle::ROTATE)) {
        m_rotateCenter = m_startRect.center();
        const QPointF point = m_view->mapToScene(viewportPos);
        m_startAngle = std::atan2(point.y() - m_rotateCenter.y(), point.x() - m_rotateCenter.x());
    }
}

void CanvasGroupBoxController::UpdateDrag(const QPoint& viewportPos)
{
    if (!m_dragging || !m_view) {
        return;
    }
    const QPointF scenePos = m_view->mapToScene(viewportPos);

    if (m_handle == static_cast<int>(GroupBoxHandle::ROTATE)) {
        const qreal angle =
            std::atan2(scenePos.y() - m_rotateCenter.y(), scenePos.x() - m_rotateCenter.x()) - m_startAngle;
        for (const MemberSnapshot& entry : std::as_const(m_snapshot)) {
            if (!entry.item) {
                continue;
            }
            entry.item->setPos(RotateAround(entry.oldPos, m_rotateCenter, angle));
            entry.item->SetRotationAngle(CanvasItem::NormalizeRotation(entry.oldRotation + angle));
        }
        return;
    }

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
    if (m_snapshot.isEmpty() || !m_scene) {
        return;
    }

    if (m_handle == static_cast<int>(GroupBoxHandle::ROTATE)) {
        QList<CanvasItem*> items;
        QList<qreal> anglesBefore;
        QList<qreal> anglesAfter;
        ItemGeometryList geometryBefore;
        ItemGeometryList geometryAfter;
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

    ItemGeometryList geometryBefore;
    ItemGeometryList geometryAfter;
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
    const QRectF rect = ViewportRect();
    if (rect.isNull()) {
        return;
    }

    QPen borderPen(QColor(79, 124, 255));
    borderPen.setWidthF(1.3);
    borderPen.setCosmetic(true);
    borderPen.setStyle(Qt::DashLine);
    borderPen.setDashPattern({5.0, 4.0});
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    QPen handlePen(QColor(79, 124, 255));
    handlePen.setWidthF(1.4);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int handle = RESIZE_HANDLES_BEGIN; handle <= RESIZE_HANDLES_END; ++handle) {
        const QRectF knob(HandleCenter(handle, rect), QSizeF(HANDLE_SIZE, HANDLE_SIZE));
        painter->drawRect(knob);
    }

    const QPointF topCenter(rect.center().x(), rect.top());
    const QPointF rotationCenter = HandleCenter(static_cast<int>(GroupBoxHandle::ROTATE), rect);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(QColor(79, 124, 255));
    const QRectF rotationKnob(rotationCenter, QSizeF(HANDLE_SIZE, HANDLE_SIZE));
    painter->drawEllipse(rotationKnob.adjusted(1.0, 1.0, -1.0, -1.0));
}
