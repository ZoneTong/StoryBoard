#include "CanvasItemInteraction.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "ShapeAbstractItem.h"
#include "shapes/ParallelogramAbstractItem.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QSet>
#include <QtMath>
#include <cmath>
#include <utility>

namespace {
constexpr qreal MINIMUM_SIDE = 8.0;

bool IsLeftHandle(int handle)
{
    return handle == CanvasItem::TOP_LEFT || handle == CanvasItem::LEFT || handle == CanvasItem::BOTTOM_LEFT;
}

bool IsRightHandle(int handle)
{
    return handle == CanvasItem::TOP_RIGHT || handle == CanvasItem::RIGHT || handle == CanvasItem::BOTTOM_RIGHT;
}

bool IsTopHandle(int handle)
{
    return handle == CanvasItem::TOP_LEFT || handle == CanvasItem::TOP || handle == CanvasItem::TOP_RIGHT;
}

bool IsBottomHandle(int handle)
{
    return handle == CanvasItem::BOTTOM_LEFT || handle == CanvasItem::BOTTOM || handle == CanvasItem::BOTTOM_RIGHT;
}

bool IsCornerHandle(int handle)
{
    return handle == CanvasItem::TOP_LEFT || handle == CanvasItem::TOP_RIGHT || handle == CanvasItem::BOTTOM_LEFT ||
           handle == CanvasItem::BOTTOM_RIGHT;
}
} // namespace

CanvasItemInteraction::CanvasItemInteraction(CanvasItem* item) : m_item(item) {}

void CanvasItemInteraction::BeginMove(const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::MOVE;
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
            if (canvasItem->GroupId() > 0) {
                groupIds.insert(canvasItem->GroupId());
            }
        }
        seen.insert(m_item);

        // 拖动任意一个组员时，把同一棵组树里的图元一并收进本次移动快照；组员不必处于
        // 选中状态，这样单个图元仍可打开属性窗并保持组内联动。
        if (CanvasScene* canvas = m_item->canvasScene()) {
            for (int groupId : std::as_const(groupIds)) {
                for (CanvasItem* member : canvas->ItemsInGroupTree(canvas->OutermostGroup(groupId))) {
                    // 正常分组都同层；若图层后续被拆分或锁定，至少不要移动
                    // 当前不可编辑的组员，避免绕过图层锁定。
                    if (canvas->IsItemEditable(member)) {
                        addItem(member);
                    }
                }
            }
        }

        for (CanvasItem* canvasItem : std::as_const(seen)) {
            m_item->m_before.append({canvasItem, canvasItem->pos(), canvasItem->ContentSize()});
        }
    }
    if (m_item->m_before.isEmpty()) {
        m_item->m_before.append({m_item, m_item->pos(), m_item->m_size});
    }
}

void CanvasItemInteraction::ApplyMove(const QPointF& scenePos)
{
    const QPointF delta = scenePos - m_item->m_dragStartScene;
    for (const ItemGeometry& geometry : std::as_const(m_item->m_before)) {
        if (geometry.item) {
            geometry.item->setPos(geometry.pos + delta);
        }
    }
}

void CanvasItemInteraction::BeginResize(int handle, const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::RESIZE;
    m_item->m_resizeHandle = static_cast<CanvasItem::Handle>(handle);
    m_item->m_dragStartScene = scenePos;
    m_item->m_dragStartPos = m_item->pos();
    m_item->m_dragStartSize = m_item->m_size;
    m_item->m_before = {{m_item, m_item->pos(), m_item->m_size}};

    if (auto* para = dynamic_cast<ParallelogramAbstractItem*>(m_item);
        handle == CanvasItem::SKEW && para && para->shapeType() == ShapeType::PARALLELOGRAM &&
        para->SkewHandleEnabled() && !para->IsCustomShape()) {
        // 内角旋钮绕左下顶点旋转上边：先记住底边的场景锚点与长度。
        // 拖动中包围框会随上边偏移量增减，不能再用固定的拖拽起点推算。
        const QRectF rect = m_item->ContentRect();
        const qreal cotValue = 1.0 / qTan(qDegreesToRadians(para->ParallelogramAngle()));
        const qreal shift = qMin(qAbs(rect.height() * cotValue), qMax<qreal>(0.0, rect.width() - 1.0));
        m_parallelogramBaseLength = qMax<qreal>(1.0, rect.width() - shift);
        const qreal bottomLeftX = cotValue >= 0.0 ? rect.left() : rect.left() + shift;
        m_parallelogramBottomLeft = m_item->mapToScene(QPointF(bottomLeftX, rect.bottom()));
    }

    if (auto* triangle = dynamic_cast<ShapeAbstractItem*>(m_item);
        triangle && triangle->shapeType() == ShapeType::TRIANGLE && !triangle->IsCustomShape() &&
        handle == CanvasItem::APEX) {
        m_triangleDragBaseA = m_item->mapToScene(triangle->triangleVertex(ShapeAbstractItem::TriangleVertex::A));
        m_triangleDragBaseB = m_item->mapToScene(triangle->triangleVertex(ShapeAbstractItem::TriangleVertex::B));
    }
}

void CanvasItemInteraction::ApplyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
{
    const QPointF delta = scenePos - m_item->m_dragStartScene;
    QRectF rect(m_item->m_dragStartPos, m_item->m_dragStartSize);
    const int handle = m_item->m_resizeHandle;

    // 需求 3：平行四边形内角由顶边中点的倾斜旋钮拖动。旋钮位于“底边中点 + 上边偏移”，
    // 因此以固定左下顶点为原点量横向距离：上边偏移 = 指针到底边中点的距离 − 底边半长。
    // 上边向左右移动即改变左下内角，底边长度全程保持不动。
    if (auto* para = dynamic_cast<ParallelogramAbstractItem*>(m_item);
        para && para->shapeType() == ShapeType::PARALLELOGRAM && para->SkewHandleEnabled() && !para->IsCustomShape() &&
        handle == CanvasItem::SKEW) {
        const qreal startHeight = m_item->m_dragStartSize.height();
        QPointF along(1.0, 0.0);
        const qreal rotation = m_item->RotationAngle();
        if (!qFuzzyIsNull(rotation)) {
            QTransform transform;
            transform.rotate(rotation);
            along = transform.map(along);
        }
        const QPointF toPointer = scenePos - m_parallelogramBottomLeft;
        const qreal signedShift = QPointF::dotProduct(toPointer, along) - m_parallelogramBaseLength / 2.0;
        const qreal angle = qRadiansToDegrees(qAtan2(startHeight, signedShift));
        para->SetParallelogramAngle(angle);
        para->SetAngleReadoutVisible(true);
        return;
    }

    // 需求 §3.1：拖顶点只改内角 A。AB 边上的垂直高度保持拖拽开始时的值，
    // B 由底边长度与高度反算（cotB = base/h − cotA）；A 的钝角分支跟指针方向
    // 一致（D12），按 1° 步进取整数。
    if (auto* triangle = dynamic_cast<ShapeAbstractItem*>(m_item);
        triangle && triangle->shapeType() == ShapeType::TRIANGLE && !triangle->IsCustomShape() &&
        handle == CanvasItem::APEX) {
        const QPointF base = m_triangleDragBaseB - m_triangleDragBaseA;
        const qreal baseLength = qMax<qreal>(1.0, std::hypot(base.x(), base.y()));
        const QPointF along = base / baseLength;
        const QPointF toPointer = scenePos - m_triangleDragBaseA;
        const qreal height = qMax<qreal>(1.0, m_item->m_dragStartSize.height());
        const qreal fromA = QPointF::dotProduct(toPointer, along);
        const qreal angleA = qRound(qRadiansToDegrees(qAtan2(height, fromA)));
        const qreal cotA = 1.0 / qTan(qDegreesToRadians(angleA));
        const qreal cotB = baseLength / height - cotA;
        const qreal angleB = qRadiansToDegrees(qAtan2(1.0, cotB));
        triangle->SetTriangleAnglesFixedHeight(angleA, angleB, height);
        triangle->SetAngleReadoutVisible(true);
        // 底边长度与高度都没变，把局部 A 点搬回拖拽开始时的场景位置即可，
        // B 会随同一条底边一起保持固定。
        const QPointF newA = m_item->mapToScene(triangle->triangleVertex(ShapeAbstractItem::TriangleVertex::A));
        m_item->setPos(m_item->pos() + (m_triangleDragBaseA - newA));
        return;
    }

    // §3.1：三角形不再等比缩放——四角 / 边走通用自由缩放，B 在 ContentResized()
    // 里按新框宽高反算；拖外接框 / 边时 A、B 读数都随拖动显示。
    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(m_item);
        shape && shape->shapeType() == ShapeType::TRIANGLE && !shape->IsCustomShape()) {
        shape->SetAngleReadoutVisible(true);
    }

    if (IsLeftHandle(handle)) {
        rect.setLeft(rect.left() + delta.x());
    }
    if (IsRightHandle(handle)) {
        rect.setRight(rect.right() + delta.x());
    }
    if (IsTopHandle(handle)) {
        rect.setTop(rect.top() + delta.y());
    }
    if (IsBottomHandle(handle)) {
        rect.setBottom(rect.bottom() + delta.y());
    }

    const qreal minSide = MINIMUM_SIDE;
    if (rect.width() < minSide) {
        if (IsLeftHandle(handle)) {
            rect.setLeft(rect.right() - minSide);
        } else {
            rect.setRight(rect.left() + minSide);
        }
    }
    if (rect.height() < minSide) {
        if (IsTopHandle(handle)) {
            rect.setTop(rect.bottom() - minSide);
        } else {
            rect.setBottom(rect.top() + minSide);
        }
    }

    if ((modifiers & Qt::ShiftModifier) && IsCornerHandle(handle) && m_item->m_dragStartSize.height() > 0.0) {
        const qreal aspect = m_item->m_dragStartSize.width() / m_item->m_dragStartSize.height();
        QSizeF size = rect.size();
        if (size.width() / size.height() > aspect) {
            size.setWidth(size.height() * aspect);
        } else {
            size.setHeight(size.width() / aspect);
        }
        size = size.expandedTo(QSizeF(minSide, minSide));

        if (IsLeftHandle(handle)) {
            rect.setLeft(rect.right() - size.width());
        } else {
            rect.setRight(rect.left() + size.width());
        }
        if (IsTopHandle(handle)) {
            rect.setTop(rect.bottom() - size.height());
        } else {
            rect.setBottom(rect.top() + size.height());
        }
    }

    // §3.2：菱形拖外接框 / 边时显示派生内角读数（尺寸是真相，角度随宽高连续变化，
    // 1 位小数显示；Shift 等比缩放时角度不变，读数维持原值）。
    if (auto* family = dynamic_cast<ParallelogramAbstractItem*>(m_item);
        family && family->shapeType() == ShapeType::DIAMOND && !family->IsCustomShape()) {
        family->SetAngleReadoutVisible(true);
    }

    m_item->setPos(rect.topLeft());
    m_item->SetContentSize(rect.size());
}

void CanvasItemInteraction::BeginRotate(const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::ROTATE;
    m_item->m_dragStartScene = scenePos;
    m_item->m_dragStartRotation = m_item->m_rotation;
    const QPointF center = m_item->mapToScene(m_item->ContentRect().center());
    m_item->m_rotateStartAngle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    m_item->m_before.clear();
}

void CanvasItemInteraction::ApplyRotate(const QPointF& scenePos)
{
    const QPointF center = m_item->mapToScene(m_item->ContentRect().center());
    const qreal angle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    const qreal target = m_item->m_dragStartRotation + (angle - m_item->m_rotateStartAngle);
    // 需求 35：旋转手柄按 5° 步长吸附。
    m_item->SetRotationAngle(CanvasItem::SnapRotation(target, 5.0));
}

void CanvasItemInteraction::FinishInteraction()
{
    if (m_item->m_drag == CanvasItem::DragKind::NONE) {
        return;
    }

    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(m_item)) {
        shape->SetAngleReadoutVisible(false);
    }

    const bool wasResize = m_item->m_drag == CanvasItem::DragKind::RESIZE;
    const bool wasRotate = m_item->m_drag == CanvasItem::DragKind::ROTATE;
    m_item->m_drag = CanvasItem::DragKind::NONE;
    m_item->m_resizeHandle = CanvasItem::NO_HANDLE;

    if (wasRotate) {
        if (CanvasScene* canvas = m_item->canvasScene()) {
            canvas->PushRotation(m_item, m_item->m_dragStartRotation, m_item->m_rotation, m_item->tr("Rotate item"));
        }
    } else if (!m_item->m_before.isEmpty()) {
        ItemGeometryList after;
        after.reserve(m_item->m_before.size());
        for (const ItemGeometry& geometry : std::as_const(m_item->m_before)) {
            if (geometry.item) {
                after.append({geometry.item, geometry.item->pos(), geometry.item->ContentSize()});
            }
        }
        if (CanvasScene* canvas = m_item->canvasScene()) {
            canvas->PushGeometry(m_item->m_before, after,
                                 wasResize ? m_item->tr("Resize item") : m_item->tr("Move item"));
        }
    }
    m_item->m_before.clear();
}

void CanvasItemInteraction::OnHoverMove(QGraphicsSceneHoverEvent* event)
{
    if (!m_item->IsEditable()) {
        m_item->unsetCursor();
        m_item->QGraphicsObject::hoverMoveEvent(event);
        return;
    }
    UpdateHoverCursor(event->pos());
    m_item->QGraphicsObject::hoverMoveEvent(event);
}

void CanvasItemInteraction::OnHoverLeave(QGraphicsSceneHoverEvent* event)
{
    m_item->unsetCursor();
    m_item->QGraphicsObject::hoverLeaveEvent(event);
}

void CanvasItemInteraction::UpdateHoverCursor(const QPointF& pos)
{
    if (!m_item->IsEditable()) {
        m_item->unsetCursor();
        return;
    }
    if (m_item->HasSingleSelection()) {
        const CanvasItem::Handle handle = m_item->HandleAt(pos);
        if (handle != CanvasItem::NO_HANDLE) {
            m_item->setCursor(m_item->CursorForHandle(handle));
            return;
        }
    }
    m_item->setCursor(QCursor(Qt::OpenHandCursor));
}

void CanvasItemInteraction::OnMousePress(QGraphicsSceneMouseEvent* event)
{
    if (!m_item->IsEditable() || event->button() != Qt::LeftButton) {
        m_item->QGraphicsObject::mousePressEvent(event);
        return;
    }

    if (m_item->HasSingleSelection()) {
        const CanvasItem::Handle handle = m_item->HandleAt(event->pos());
        if (handle == CanvasItem::ROTATION) {
            BeginRotate(event->scenePos());
            m_item->setCursor(QCursor(Qt::PointingHandCursor));
            event->accept();
            return;
        }
        if (handle != CanvasItem::NO_HANDLE) {
            BeginResize(handle, event->scenePos());
            m_item->setCursor(m_item->CursorForHandle(handle));
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

    BeginMove(event->scenePos());
    m_item->setCursor(QCursor(Qt::ClosedHandCursor));
    event->accept();
}

void CanvasItemInteraction::OnMouseMove(QGraphicsSceneMouseEvent* event)
{
    if (m_item->m_drag == CanvasItem::DragKind::NONE) {
        m_item->QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    if (m_item->m_drag == CanvasItem::DragKind::MOVE) {
        ApplyMove(event->scenePos());
    } else if (m_item->m_drag == CanvasItem::DragKind::RESIZE) {
        ApplyResize(event->scenePos(), event->modifiers());
    } else if (m_item->m_drag == CanvasItem::DragKind::ROTATE) {
        ApplyRotate(event->scenePos());
    }

    event->accept();
}

void CanvasItemInteraction::OnMouseRelease(QGraphicsSceneMouseEvent* event)
{
    // 需求 23：图元点击的选择逻辑在 mousePress 已完整处理（含 Ctrl 加减选）。
    // 未进入拖拽时不要放行给 QGraphicsItem 默认 release——它会在松开时再做一次
    // “点击选择”（Ctrl 下重复切换、无 Ctrl 下替换多选），把按下时的选择结果破坏掉。
    if (m_item->m_drag == CanvasItem::DragKind::NONE) {
        event->accept();
        return;
    }

    FinishInteraction();
    UpdateHoverCursor(event->pos());
    event->accept();
}
