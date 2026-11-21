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
    CanvasItem* m_item = nullptr;
    // 拖三角形顶点 C 时固定底边：记录拖拽开始时的 A/B 场景位置。
    QPointF m_triangleDragBaseA;
    QPointF m_triangleDragBaseB;
    // 拖平行四边形内角旋钮时固定底边：记录左下顶点场景位置与底边长度。
    QPointF m_parallelogramBottomLeft;
    qreal m_parallelogramBaseLength = 0.0;
};
