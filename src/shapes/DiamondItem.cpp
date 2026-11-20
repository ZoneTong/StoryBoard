#include "shapes/DiamondItem.h"

#include "ShapePathBuilder.h"

DiamondItem::DiamondItem()
{
    setContentSize(defaultShapeSize(ShapeType::Diamond));
}

QPainterPath DiamondItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Diamond, rect, -1.0);
}
