#include "shapes/HeartItem.h"

#include "ShapePathBuilder.h"

HeartItem::HeartItem()
{
    setContentSize(defaultContentSize());
}

QSizeF HeartItem::defaultContentSize() const
{
    return QSizeF(128.0, 112.0);
}

QPainterPath HeartItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::heartPath(rect);
}
