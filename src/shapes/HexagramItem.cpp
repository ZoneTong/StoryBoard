#include "shapes/HexagramItem.h"

#include "ShapePathBuilder.h"

HexagramItem::HexagramItem()
{
    setContentSize(defaultShapeSize(ShapeType::Hexagram));
}

QPainterPath HexagramItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Hexagram, rect, -1.0);
}
