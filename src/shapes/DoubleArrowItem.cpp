#include "shapes/DoubleArrowItem.h"

#include "ShapePathBuilder.h"

DoubleArrowItem::DoubleArrowItem()
{
    setContentSize(defaultContentSize());
}

QSizeF DoubleArrowItem::defaultContentSize() const
{
    return QSizeF(128.0, 40.0);
}

QPainterPath DoubleArrowItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::doubleArrowPath(rect);
}
