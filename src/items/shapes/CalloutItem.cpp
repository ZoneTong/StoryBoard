#include "items/shapes/CalloutItem.h"

#include "items/geometry/ShapePathBuilder.h"

CalloutItem::CalloutItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF CalloutItem::DefaultContentSize() const
{
    return QSizeF(128.0, 86.0);
}

QPainterPath CalloutItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::CalloutPath(rect);
}
