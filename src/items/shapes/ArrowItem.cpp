#include "items/shapes/ArrowItem.h"

#include "items/geometry/ShapePathBuilder.h"

ArrowItem::ArrowItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF ArrowItem::DefaultContentSize() const
{
    return QSizeF(128.0, 46.0);
}

QPainterPath ArrowItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::ArrowPath(rect);
}
