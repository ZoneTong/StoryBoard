#include "scene/ItemHeightPlanner.h"

#include "CanvasItem.h"

#include <QSet>

ItemHeightPlanner::ItemHeightPlanner(int maxHeight) : m_maxHeight(maxHeight) {}

int ItemHeightPlanner::nextHeight(const QList<CanvasItem*>& items, int layerId) const
{
    QSet<int> used;
    int maximum = 0;
    for (CanvasItem* item : items) {
        if (item->LayerId() != layerId || item->ZOrder() < 0) {
            continue;
        }
        used.insert(item->ZOrder());
        maximum = qMax(maximum, item->ZOrder());
    }

    const int aboveTop = maximum + 1;
    if (aboveTop <= m_maxHeight && !used.contains(aboveTop)) {
        return aboveTop;
    }

    // 图层实际已满（或触到上限）：回填第一个空位，而不是发出去一个重复高度。
    for (int height = 0; height <= m_maxHeight; ++height) {
        if (!used.contains(height)) {
            return height;
        }
    }
    return -1;
}

bool ItemHeightPlanner::heightsUnique(const QList<CanvasItem*>& items) const
{
    QSet<int> heights;
    for (CanvasItem* item : items) {
        if (item->ZOrder() < 0 || item->ZOrder() > m_maxHeight || heights.contains(item->ZOrder())) {
            return false;
        }
        heights.insert(item->ZOrder());
    }
    return true;
}

bool ItemHeightPlanner::heightUsedByOther(const QList<CanvasItem*>& items, int height, const CanvasItem* except) const
{
    for (const CanvasItem* item : items) {
        if (item != except && item->ZOrder() == height) {
            return true;
        }
    }
    return false;
}
