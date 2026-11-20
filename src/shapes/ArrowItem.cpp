#include "shapes/ArrowItem.h"

#include "ShapePathBuilder.h"

ArrowItem::ArrowItem()
{
    setContentSize(defaultContentSize());
}

QSizeF ArrowItem::defaultContentSize() const
{
    return QSizeF(128.0, 46.0);
}

QPainterPath ArrowItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::arrowPath(rect);
}
