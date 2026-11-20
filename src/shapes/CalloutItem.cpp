#include "shapes/CalloutItem.h"

#include "ShapePathBuilder.h"

CalloutItem::CalloutItem()
{
    setContentSize(defaultContentSize());
}

QSizeF CalloutItem::defaultContentSize() const
{
    return QSizeF(128.0, 86.0);
}

QPainterPath CalloutItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::calloutPath(rect);
}
