#pragma once

#include <QPointF>
#include <QSizeF>
#include <Qt>

class CanvasItem;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

// 图元拖动交互：移动（含成组联动快照）、缩放（含最小边长与等比约束）、旋转（含步长吸附）。
// 同时承接悬停与鼠标事件分发：交互开始时记录快照，结束时通过场景推入撤销记录；
// 图元本身只保留交互状态。
class CanvasItemInteraction {
  public:
    explicit CanvasItemInteraction(CanvasItem* item);

    void beginMove(const QPointF& scenePos);
    void applyMove(const QPointF& scenePos);
    void beginResize(int handle, const QPointF& scenePos);
    void applyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers);
    void beginRotate(const QPointF& scenePos);
    void applyRotate(const QPointF& scenePos);
    void finishInteraction();

    void onHoverMove(QGraphicsSceneHoverEvent* event);
    void onHoverLeave(QGraphicsSceneHoverEvent* event);
    void onMousePress(QGraphicsSceneMouseEvent* event);
    void onMouseMove(QGraphicsSceneMouseEvent* event);
    void onMouseRelease(QGraphicsSceneMouseEvent* event);
    void updateHoverCursor(const QPointF& pos);

  private:
    CanvasItem* m_item = nullptr;
};
