#pragma once

#include <QPoint>

class CanvasItem;
class CanvasScene;
class CanvasView;

// 视图坐标下的图元命中查询：按可见性与真实叠放顺序、图层可编辑性、优先图层偏好
// 自上而下查找。只做查询，不改变选择状态。
class CanvasViewHitTester {
  public:
    explicit CanvasViewHitTester(CanvasView* view, CanvasScene* scene);

    CanvasItem* TopItemAt(const QPoint& viewportPos) const;
    CanvasItem* PreferredEditableItemAt(const QPoint& viewportPos) const;
    CanvasItem* EditableItemAt(const QPoint& viewportPos) const;

  private:
    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
};
