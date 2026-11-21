#include "shapes/CylinderItem.h"

#include "ShapePathBuilder.h"

CylinderItem::CylinderItem()
{
    SetContentSize(defaultContentSize());
}

QSizeF CylinderItem::defaultContentSize() const
{
    return QSizeF(104.0, 128.0);
}

QPainterPath CylinderItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::cylinderPath(rect);
}
