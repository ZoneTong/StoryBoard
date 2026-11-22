#include "items/shapes/DoubleArrowItem.h"

#include "items/geometry/ShapePathBuilder.h"

DoubleArrowItem::DoubleArrowItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF DoubleArrowItem::DefaultContentSize() const
{
    return QSizeF(128.0, 40.0);
}

QPainterPath DoubleArrowItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::DoubleArrowPath(rect);
}
