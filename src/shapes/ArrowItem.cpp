#include "shapes/ArrowItem.h"

#include "ShapePathBuilder.h"

ArrowItem::ArrowItem()
{
    setContentSize(defaultShapeSize(ShapeType::Arrow));
}

QPainterPath ArrowItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Arrow, rect, -1.0);
}
