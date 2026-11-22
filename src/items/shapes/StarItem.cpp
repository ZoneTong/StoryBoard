#include "items/shapes/StarItem.h"

#include "items/geometry/ShapePathBuilder.h"

StarItem::StarItem()
{
    SetContentSize(DefaultContentSize());
}

QPainterPath StarItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::StarPath(rect);
}
