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

    CanvasItem* topItemAt(const QPoint& viewportPos) const;
    // 只按可见性和叠放顺序命中，不检查工具模式或图层锁定；属性窗自己决定是否可编辑。
    CanvasItem* propertyItemAt(const QPoint& viewportPos) const;
    CanvasItem* preferredEditableItemAt(const QPoint& viewportPos) const;
    CanvasItem* editableItemAt(const QPoint& viewportPos) const;

  private:
    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
};
