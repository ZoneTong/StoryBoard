#include "items/shapes/CylinderItem.h"

#include "items/geometry/ShapePathBuilder.h"

CylinderItem::CylinderItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF CylinderItem::DefaultContentSize() const
{
    return QSizeF(104.0, 128.0);
}

QPainterPath CylinderItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::CylinderPath(rect);
}
