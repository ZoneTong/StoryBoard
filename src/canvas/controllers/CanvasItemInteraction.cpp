#include "canvas/controllers/CanvasItemInteraction.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"

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
    for (const ItemPlacement& geometry : std::as_const(m_item->m_before)) {
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
    m_item->m_dragStartFrame = m_item->FrameRect();
    m_item->m_before = {{m_item, m_item->pos(), m_item->m_size}};
    m_customDrag = false;
    m_customDragBefore.clear();
    m_lineCustomDrag = nullptr;

    if (handle < CanvasItem::CUSTOM_0) {
        return;
    }
    // P2：自定义句柄的拖拽语义由图元自己实现，控制器不再按图元类型分支。
    m_customDrag = m_item->BeginCustomHandleDrag(handle, scenePos);
    if (!m_customDrag) {
        return;
    }
    if (auto* line = dynamic_cast<LineAbstractItem*>(m_item)) {
        m_lineCustomDrag = line;
        m_lineGeometryBefore = line->geometryState();
        m_hasBindingSnapshot = true;
        m_startBindingBefore = line->StartBinding();
        m_endBindingBefore = line->EndBinding();
    }
    // 角度 / 端点这类属性不在 GeometryCommand 的回放范围里，改为逐属性快照，
    // 拖拽结束时只把真正变过的项推成一条 ItemPropertyCommand。
    for (PropertyKey property : m_item->Properties().Keys()) {
        m_customDragBefore.insert(property, m_item->PropertyValueByKey(property));
    }
}

void CanvasItemInteraction::ApplyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
{
    const int handle = m_item->m_resizeHandle;

    if (m_customDrag) {
        m_item->ApplyCustomHandleDrag(handle, scenePos);
        return;
    }

    // 拖外接框 / 边时的逐帧回调：三角形（A/B）与菱形（派生内角）在这里点亮读数
    // （§3.1、§3.2），控制器不再按图元类型分支。
    m_item->FrameResizeDragged();

    const QPointF delta = scenePos - m_item->m_dragStartScene;
    // 缩放基准是选中框（FrameRect），不是内容框：弧线的框是椭圆外接框，比笔迹包围盒大。
    // 框与内容框同尺寸的图元偏移为 0，下面的换算自然退化成旧行为。
    const QRectF startFrame = m_item->m_dragStartFrame;
    QRectF rect(startFrame.topLeft() + m_item->m_dragStartPos, startFrame.size());

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
        const qreal aspect = startFrame.width() / startFrame.height();
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

    const qreal scaleX = startFrame.width() > 0.0 ? rect.width() / startFrame.width() : 1.0;
    const qreal scaleY = startFrame.height() > 0.0 ? rect.height() / startFrame.height() : 1.0;
    QSizeF size(m_item->m_dragStartSize.width() * scaleX, m_item->m_dragStartSize.height() * scaleY);
    size = size.expandedTo(QSizeF(MINIMUM_SIDE, MINIMUM_SIDE));

    // 内容原点相对选中框左上角的偏移随缩放一起变，否则弧线这类“框大于内容框”的图元
    // 在拖角时会把椭圆整体平移出去。
    const QPointF frameOffset = startFrame.topLeft();
    m_item->setPos(rect.topLeft() - QPointF(frameOffset.x() * scaleX, frameOffset.y() * scaleY));
    m_item->SetContentSize(size);
}

void CanvasItemInteraction::BeginRotate(const QPointF& scenePos)
{
    m_item->m_drag = CanvasItem::DragKind::ROTATE;
    m_item->m_dragStartScene = scenePos;
    m_item->m_dragStartRotation = m_item->m_rotation;
    const QPointF center = m_item->mapToScene(m_item->RotationOrigin());
    m_item->m_rotateStartAngle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    m_item->m_before.clear();
}

void CanvasItemInteraction::ApplyRotate(const QPointF& scenePos)
{
    const QPointF center = m_item->mapToScene(m_item->RotationOrigin());
    const qreal angle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    const qreal target = m_item->m_dragStartRotation + (angle - m_item->m_rotateStartAngle);
    // 旋转手柄按 1° 步长吸附：连续拖动也只落在整度上，与编辑面板的 1° 步长一致。
    m_item->SetRotationAngle(CanvasItem::SnapRotation(target, 1.0));
}

void CanvasItemInteraction::FinishInteraction()
{
    if (m_item->m_drag == CanvasItem::DragKind::NONE) {
        return;
    }

    // 收起拖动中的临时状态（角度读数等），由各图元自己决定要不要处理。
    m_item->EndHandleDrag();
    if (CanvasScene* canvas = m_item->canvasScene()) {
        canvas->SetBindingHighlight(nullptr);
    }

    const bool wasResize = m_item->m_drag == CanvasItem::DragKind::RESIZE;
    const bool wasRotate = m_item->m_drag == CanvasItem::DragKind::ROTATE;
    const bool wasCustomDrag = wasResize && m_customDrag;
    const QMap<PropertyKey, QVariant> customBefore = m_customDragBefore;
    LineAbstractItem* lineCustomDrag = m_lineCustomDrag;
    const LineAbstractItem::GeometryState lineGeometryBefore = m_lineGeometryBefore;
    const bool hasBindingSnapshot = m_hasBindingSnapshot;
    const QJsonObject startBindingBefore = m_startBindingBefore;
    const QJsonObject endBindingBefore = m_endBindingBefore;
    m_item->m_drag = CanvasItem::DragKind::NONE;
    m_item->m_resizeHandle = CanvasItem::NO_HANDLE;
    m_customDrag = false;
    m_customDragBefore.clear();
    m_lineCustomDrag = nullptr;
    m_hasBindingSnapshot = false;

    if (wasRotate) {
        if (CanvasScene* canvas = m_item->canvasScene()) {
            canvas->PushRotation(m_item, m_item->m_dragStartRotation, m_item->m_rotation, m_item->tr("Rotate item"));
        }
    } else if (wasCustomDrag) {
        if (lineCustomDrag) {
            const LineAbstractItem::GeometryState after = lineCustomDrag->geometryState();
            const bool geometryChanged = after != lineGeometryBefore;
            const bool bindingChanged =
                hasBindingSnapshot && (startBindingBefore != lineCustomDrag->StartBinding() ||
                                       endBindingBefore != lineCustomDrag->EndBinding());
            if (CanvasScene* canvas = m_item->canvasScene(); canvas && canvas->UndoStack() &&
                                                              (geometryChanged || bindingChanged)) {
                const QString text = m_item->tr("Move endpoint");
                if (geometryChanged && bindingChanged) {
                    canvas->UndoStack()->beginMacro(text);
                }
                if (geometryChanged) {
                    canvas->UndoStack()->push(new StrokeSmoothCommand(canvas, {lineCustomDrag},
                                                                      {lineGeometryBefore}, {after}, text));
                }
                if (bindingChanged) {
                    canvas->UndoStack()->push(
                        new ArrowBindingCommand(canvas, {lineCustomDrag},
                                                {ArrowBindingState{startBindingBefore, endBindingBefore}},
                                                {ArrowBindingState{lineCustomDrag->StartBinding(),
                                                                   lineCustomDrag->EndBinding()}},
                                                m_item->tr("Rebind arrow")));
                }
                if (geometryChanged && bindingChanged) {
                    canvas->UndoStack()->endMacro();
                }
            }
        } else {
            PushCustomHandleChange(customBefore);
        }
    } else if (!m_item->m_before.isEmpty()) {
        ItemPlacementList after;
        after.reserve(m_item->m_before.size());
        for (const ItemPlacement& geometry : std::as_const(m_item->m_before)) {
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

void CanvasItemInteraction::PushCustomHandleChange(const QMap<PropertyKey, QVariant>& before)
{
    CanvasScene* canvas = m_item->canvasScene();
    if (!canvas || !canvas->UndoStack()) {
        return;
    }
    QMap<PropertyKey, QVariant> beforeChanged;
    QMap<PropertyKey, QVariant> afterChanged;
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        const QVariant now = m_item->PropertyValueByKey(it.key());
        if (now == it.value()) {
            continue;
        }
        beforeChanged.insert(it.key(), it.value());
        afterChanged.insert(it.key(), now);
    }
    if (beforeChanged.isEmpty()) {
        return;
    }
    canvas->UndoStack()->push(
        new ItemPropertyCommand(canvas, {m_item}, {beforeChanged}, {afterChanged}, m_item->tr("Resize item")));
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
