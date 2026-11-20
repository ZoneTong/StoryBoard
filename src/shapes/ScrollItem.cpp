#include "shapes/ScrollItem.h"

#include "ShapePathBuilder.h"

ScrollItem::ScrollItem()
{
    setContentSize(defaultContentSize());
}

QSizeF ScrollItem::defaultContentSize() const
{
    return QSizeF(128.0, 96.0);
}

QPainterPath ScrollItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::scrollPath(rect);
}
