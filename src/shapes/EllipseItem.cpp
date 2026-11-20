#include "shapes/EllipseItem.h"

#include "ShapePathBuilder.h"

EllipseItem::EllipseItem()
{
    setContentSize(defaultContentSize());
}

QPainterPath EllipseItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::ellipsePath(rect);
}
