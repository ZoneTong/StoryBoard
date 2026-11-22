#include "CanvasItemInteraction.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "ShapeAbstractItem.h"

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

    if (auto* para = dynamic_cast<ShapeAbstractItem*>(m_item); handle == CanvasItem::SKEW && para &&
                                                               para->shapeType() == ShapeType::PARALLELOGRAM &&
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
    if (auto* para = dynamic_cast<ShapeAbstractItem*>(m_item); para && para->shapeType() == ShapeType::PARALLELOGRAM &&
                                                               para->SkewHandleEnabled() && !para->IsCustomShape() &&
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

    // 需求 2：拖顶点 C 只改三角形，AB 两端在场景中固定；底边长度不变，
    // 顶点可以越过 A / B 的垂线，因此两个底角都能进入钝角区。
    if (auto* triangle = dynamic_cast<ShapeAbstractItem*>(m_item);
        triangle && triangle->shapeType() == ShapeType::TRIANGLE && !triangle->IsCustomShape() &&
        handle == CanvasItem::APEX) {
        const QPointF base = m_triangleDragBaseB - m_triangleDragBaseA;
        const qreal baseLength = qMax<qreal>(1.0, std::hypot(base.x(), base.y()));
        const QPointF along = base / baseLength;
        // 未旋转时 along=(1,0)，normal=(0,-1) 指向画布上方；旋转后一起跟着底边转。
        const QPointF normal(along.y(), -along.x());
        const QPointF toPointer = scenePos - m_triangleDragBaseA;
        const qreal height = qMax<qreal>(0.0, QPointF::dotProduct(toPointer, normal));
        const qreal fromA = QPointF::dotProduct(toPointer, along);
        const qreal fromB = QPointF::dotProduct(scenePos - m_triangleDragBaseB, along);
        const qreal angleA = qRadiansToDegrees(qAtan2(height, fromA));
        const qreal angleB = qRadiansToDegrees(qAtan2(height, -fromB));
        triangle->SetTriangleAngles(angleA, angleB);
        triangle->SetAngleReadoutVisible(true);
        // 角度与包围盒重算后，把局部 A 点搬回拖拽开始时的场景位置；底边长度不变，
        // B 会随同一条底边一起保持固定。
        const QPointF newA = m_item->mapToScene(triangle->triangleVertex(ShapeAbstractItem::TriangleVertex::A));
        m_item->setPos(m_item->pos() + (m_triangleDragBaseA - newA));
        return;
    }

    // 三角形拖动包围框只能等比缩放：三个内角保持不变，手柄对面的那个点钉住不动
    // （角对手柄钉对角、边中点对手柄钉对边中点）。高度由底边与角度派生，缩放后
    // 宽高比自然保持，所以只要按比例算新尺寸、再把锚点摆回原处。
    if (auto* triangle = dynamic_cast<ShapeAbstractItem*>(m_item);
        triangle && triangle->shapeType() == ShapeType::TRIANGLE && !triangle->IsCustomShape()) {
        const QSizeF startSize = m_item->m_dragStartSize;
        // 锚点在内容矩形里的归一化位置（0 = 左 / 上，1 = 右 / 下）。
        qreal anchorX = 0.5;
        qreal anchorY = 0.5;
        switch (handle) {
        case CanvasItem::TOP_LEFT:
            anchorX = 1.0;
            anchorY = 1.0;
            break;
        case CanvasItem::TOP:
            anchorY = 1.0;
            break;
        case CanvasItem::TOP_RIGHT:
            anchorX = 0.0;
            anchorY = 1.0;
            break;
        case CanvasItem::LEFT:
            anchorX = 1.0;
            break;
        case CanvasItem::RIGHT:
            anchorX = 0.0;
            break;
        case CanvasItem::BOTTOM_LEFT:
            anchorX = 1.0;
            anchorY = 0.0;
            break;
        case CanvasItem::BOTTOM:
            anchorY = 0.0;
            break;
        case CanvasItem::BOTTOM_RIGHT:
            anchorX = 0.0;
            anchorY = 0.0;
            break;
        default:
            break;
        }
        const bool horizontal = IsLeftHandle(handle) || IsRightHandle(handle);
        const bool vertical = IsTopHandle(handle) || IsBottomHandle(handle);
        const qreal kx =
            horizontal ? 1.0 + (IsLeftHandle(handle) ? -delta.x() : delta.x()) / qMax<qreal>(1.0, startSize.width())
                       : 1.0;
        const qreal ky =
            vertical ? 1.0 + (IsTopHandle(handle) ? -delta.y() : delta.y()) / qMax<qreal>(1.0, startSize.height())
                     : 1.0;
        // 角手柄两维都有位移，跟位移更大的那一维走；边手柄只有一维。
        qreal scale = horizontal && vertical ? (qAbs(kx - 1.0) >= qAbs(ky - 1.0) ? kx : ky) : (horizontal ? kx : ky);
        scale = qMax(scale, qMax(MINIMUM_SIDE / qMax<qreal>(1.0, startSize.width()),
                                 MINIMUM_SIDE / qMax<qreal>(1.0, startSize.height())));
        const qreal newWidth = startSize.width() * scale;
        const qreal cot1 = 1.0 / qTan(qDegreesToRadians(triangle->BottomAngle1()));
        const qreal cot2 = 1.0 / qTan(qDegreesToRadians(triangle->BottomAngle2()));
        const qreal newHeight = newWidth / qMax<qreal>(0.02, cot1 + cot2);
        const QPointF anchorScene =
            m_item->m_dragStartPos + QPointF(anchorX * startSize.width(), anchorY * startSize.height());
        m_item->setPos(anchorScene - QPointF(anchorX * newWidth, anchorY * newHeight));
        m_item->SetContentSize(QSizeF(newWidth, newHeight));
        return;
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
