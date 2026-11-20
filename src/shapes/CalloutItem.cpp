#include "shapes/CalloutItem.h"

#include "ShapePathBuilder.h"

CalloutItem::CalloutItem()
{
    setContentSize(defaultShapeSize(ShapeType::Callout));
}

QPainterPath CalloutItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Callout, rect, -1.0);
}
