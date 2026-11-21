#include "shapes/CalloutItem.h"

#include "ShapePathBuilder.h"

CalloutItem::CalloutItem()
{
    SetContentSize(defaultContentSize());
}

QSizeF CalloutItem::defaultContentSize() const
{
    return QSizeF(128.0, 86.0);
}

QPainterPath CalloutItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::calloutPath(rect);
}
