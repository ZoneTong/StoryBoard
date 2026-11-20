#include "shapes/Star4Item.h"

#include "ShapePathBuilder.h"

Star4Item::Star4Item()
{
    setContentSize(defaultContentSize());
}

QPainterPath Star4Item::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::fourPointStarPath(rect);
}
