#include "items/shapes/ScrollItem.h"

#include "items/geometry/ShapePathBuilder.h"

ScrollItem::ScrollItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF ScrollItem::DefaultContentSize() const
{
    return QSizeF(128.0, 96.0);
}

QPainterPath ScrollItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::ScrollPath(rect);
}
