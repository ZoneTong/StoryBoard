#pragma once

#include <QList>
#include <QPointF>
#include <QPointer>
#include <QSizeF>

class BoardItem;

struct ItemGeometry {
    QPointer<BoardItem> item;
    QPointF pos;
    QSizeF size;
};

using ItemGeometryList = QList<ItemGeometry>;

struct ItemStack {
    QPointer<BoardItem> item;
    int layerId = 1;
    int height = 1;
};

using ItemStackList = QList<ItemStack>;
