#include "shapes/HeartItem.h"

#include "ShapePathBuilder.h"

HeartItem::HeartItem()
{
    SetContentSize(defaultContentSize());
}

QSizeF HeartItem::defaultContentSize() const
{
    return QSizeF(128.0, 112.0);
}

QPainterPath HeartItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::heartPath(rect);
}
