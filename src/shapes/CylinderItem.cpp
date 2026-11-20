#include "shapes/CylinderItem.h"

#include "ShapePathBuilder.h"

CylinderItem::CylinderItem()
{
    setContentSize(defaultContentSize());
}

QSizeF CylinderItem::defaultContentSize() const
{
    return QSizeF(104.0, 128.0);
}

QPainterPath CylinderItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::cylinderPath(rect);
}
