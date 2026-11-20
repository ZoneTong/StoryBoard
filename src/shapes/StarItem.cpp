#include "shapes/StarItem.h"

#include "ShapePathBuilder.h"

StarItem::StarItem()
{
    setContentSize(defaultContentSize());
}

QPainterPath StarItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::starPath(rect);
}
