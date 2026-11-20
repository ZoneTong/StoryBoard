#include "shapes/EllipseItem.h"

#include "ShapePathBuilder.h"

EllipseItem::EllipseItem()
{
    setContentSize(defaultShapeSize(ShapeType::Ellipse));
}

QPainterPath EllipseItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Ellipse, rect, -1.0);
}
