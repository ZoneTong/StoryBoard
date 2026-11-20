#include "shapes/DoubleArrowItem.h"

#include "ShapePathBuilder.h"

DoubleArrowItem::DoubleArrowItem()
{
    setContentSize(defaultShapeSize(ShapeType::DoubleArrow));
}

QPainterPath DoubleArrowItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::DoubleArrow, rect, -1.0);
}
