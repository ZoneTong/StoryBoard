#include "shapes/Star4Item.h"

#include "ShapePathBuilder.h"

Star4Item::Star4Item()
{
    SetContentSize(defaultContentSize());
}

QPainterPath Star4Item::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::fourPointStarPath(rect);
}
