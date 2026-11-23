#pragma once

#include <QList>
#include <QPointF>
#include <QPointer>
#include <QSizeF>

class CanvasItem;

// 图元的「位置快照」：撤销 / 对齐 / 排版只关心这三项，
// 与叠放顺序（ItemStack）分开，避免改一个概念时牵动整条调用链。
struct ItemPlacement {
    QPointer<CanvasItem> item;
    QPointF pos;
    QSizeF size;
};

using ItemPlacementList = QList<ItemPlacement>;
