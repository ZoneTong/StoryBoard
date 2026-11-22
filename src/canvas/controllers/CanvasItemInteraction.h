#pragma once

#include "items/properties/ItemProperties.h"

#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QVariant>
#include <Qt>

class CanvasItem;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

// 图元拖动交互：移动（含成组联动快照）、缩放（含最小边长与等比约束）、旋转（含步长吸附），
// 以及由图元自己实现的“自定义句柄拖拽”（角度 / 端点等）。同时承接悬停与鼠标事件分发：
// 交互开始时记录快照，结束时通过场景推入撤销记录；图元本身只保留交互状态。
class CanvasItemInteraction {
  public:
    explicit CanvasItemInteraction(CanvasItem* item);

    void BeginMove(const QPointF& scenePos);
    void ApplyMove(const QPointF& scenePos);
    void BeginResize(int handle, const QPointF& scenePos);
    void ApplyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers);
    void BeginRotate(const QPointF& scenePos);
    void ApplyRotate(const QPointF& scenePos);
    void FinishInteraction();

    void OnHoverMove(QGraphicsSceneHoverEvent* event);
    void OnHoverLeave(QGraphicsSceneHoverEvent* event);
    void OnMousePress(QGraphicsSceneMouseEvent* event);
    void OnMouseMove(QGraphicsSceneMouseEvent* event);
    void OnMouseRelease(QGraphicsSceneMouseEvent* event);
    void UpdateHoverCursor(const QPointF& pos);

  private:
    // 自定义句柄拖拽只改角度 / 端点这类派生属性，GeometryCommand 回放不了，
    // 因此按属性快照把真正变过的项推成一条 ItemPropertyCommand。
    void PushCustomHandleChange(const QMap<ItemProperty, QVariant>& before);

    CanvasItem* m_item = nullptr;
    bool m_customDrag = false;
    QMap<ItemProperty, QVariant> m_customDragBefore;
};
