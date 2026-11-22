#include "items/shapes/Star4Item.h"

#include "items/geometry/ShapePathBuilder.h"

Star4Item::Star4Item()
{
    SetContentSize(DefaultContentSize());
}

QPainterPath Star4Item::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::FourPointStarPath(rect);
}
