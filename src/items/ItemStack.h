#pragma once

#include <QList>
#include <QPointer>

class CanvasItem;

// 图元的「层内叠放快照」：改图层 / 改层内高度的撤销与修复都按这个三元组回滚。
struct ItemStack {
    QPointer<CanvasItem> item;
    int layerId = 1;
    int height = 1;
};

using ItemStackList = QList<ItemStack>;
