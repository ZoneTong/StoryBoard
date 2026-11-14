#pragma once

#include "scene/SceneConstants.h"

#include <QList>

class CanvasItem;

// 同一图层内图元高度（叠放次序）的唯一性规划：下一个可用高度、重复高度检查、
// 同层高度是否已唯一。不持有场景状态，只对传入的图元集合做纯计算，可在任意 TU 复用。
class ItemHeightPlanner {
  public:
    explicit ItemHeightPlanner(int maxHeight = scene_constants::kMaxItemHeight);

    // 取该图层下一个不与现有高度冲突的高度；图层接近满员时返回第一个空位，
    // 实在没有空位返回 -1。
    int nextHeight(const QList<CanvasItem*>& items, int layerId) const;
    // 该图层内高度是否唯一且全部落在合法区间。
    bool heightsUnique(const QList<CanvasItem*>& items) const;
    // 指定高度是否被除 except 外的图元占用。
    bool heightUsedByOther(const QList<CanvasItem*>& items, int height, const CanvasItem* except) const;

  private:
    int m_maxHeight;
};
