#include "shapes/StarItem.h"

#include "ShapePathBuilder.h"

StarItem::StarItem()
{
    SetContentSize(defaultContentSize());
}

QPainterPath StarItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::starPath(rect);
}
