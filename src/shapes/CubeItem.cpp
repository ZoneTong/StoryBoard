#include "shapes/CubeItem.h"

#include "ShapePathBuilder.h"

CubeItem::CubeItem()
{
    setContentSize(defaultShapeSize(ShapeType::Cube));
}

QPainterPath CubeItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Cube, rect, -1.0);
}
