#include "shapes/HeartItem.h"

#include "ShapePathBuilder.h"

HeartItem::HeartItem()
{
    setContentSize(defaultShapeSize(ShapeType::Heart));
}

QPainterPath HeartItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Heart, rect, -1.0);
}
