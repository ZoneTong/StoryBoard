#include "shapes/Star4Item.h"

#include "ShapePathBuilder.h"

Star4Item::Star4Item()
{
    setContentSize(defaultShapeSize(ShapeType::Star4));
}

QPainterPath Star4Item::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Star4, rect, -1.0);
}
