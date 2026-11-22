#include "items/shapes/CubeItem.h"

#include "items/geometry/ShapePathBuilder.h"

CubeItem::CubeItem()
{
    SetContentSize(DefaultContentSize());
}

QPainterPath CubeItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::CubePath(rect);
}
