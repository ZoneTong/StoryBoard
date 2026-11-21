#include "shapes/DoubleArrowItem.h"

#include "ShapePathBuilder.h"

DoubleArrowItem::DoubleArrowItem()
{
    SetContentSize(defaultContentSize());
}

QSizeF DoubleArrowItem::defaultContentSize() const
{
    return QSizeF(128.0, 40.0);
}

QPainterPath DoubleArrowItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::doubleArrowPath(rect);
}
