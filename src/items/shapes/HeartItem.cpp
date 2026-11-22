#include "items/shapes/HeartItem.h"

#include "items/geometry/ShapePathBuilder.h"

HeartItem::HeartItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF HeartItem::DefaultContentSize() const
{
    return QSizeF(128.0, 112.0);
}

QPainterPath HeartItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::HeartPath(rect);
}
