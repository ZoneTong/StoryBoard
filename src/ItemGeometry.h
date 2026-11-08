#pragma once

#include <QList>
#include <QPointF>
#include <QPointer>
#include <QSizeF>

class CanvasItem;

struct ItemGeometry {
    QPointer<CanvasItem> item;
    QPointF pos;
    QSizeF size;
};

using ItemGeometryList = QList<ItemGeometry>;

struct ItemStack {
    QPointer<CanvasItem> item;
    int layerId = 1;
    int height = 1;
};

using ItemStackList = QList<ItemStack>;
