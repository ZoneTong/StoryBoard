#include "shapes/CylinderItem.h"

#include "ShapePathBuilder.h"

CylinderItem::CylinderItem()
{
    setContentSize(defaultShapeSize(ShapeType::Cylinder));
}

QPainterPath CylinderItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Cylinder, rect, -1.0);
}
