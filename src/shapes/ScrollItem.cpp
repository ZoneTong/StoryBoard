#include "shapes/ScrollItem.h"

#include "ShapePathBuilder.h"

ScrollItem::ScrollItem()
{
    setContentSize(defaultShapeSize(ShapeType::Scroll));
}

QPainterPath ScrollItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Scroll, rect, -1.0);
}
