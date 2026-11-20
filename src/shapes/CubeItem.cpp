#include "shapes/CubeItem.h"

#include "ShapePathBuilder.h"

CubeItem::CubeItem()
{
    setContentSize(defaultContentSize());
}

QPainterPath CubeItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::cubePath(rect);
}
