#include "shapes/StarItem.h"

#include "ShapePathBuilder.h"

StarItem::StarItem()
{
    setContentSize(defaultShapeSize(ShapeType::Star));
}

QPainterPath StarItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Star, rect, -1.0);
}
