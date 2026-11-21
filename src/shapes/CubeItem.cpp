#include "shapes/CubeItem.h"

#include "ShapePathBuilder.h"

CubeItem::CubeItem()
{
    SetContentSize(defaultContentSize());
}

QPainterPath CubeItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::cubePath(rect);
}
