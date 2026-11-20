#include "shapes/HexagramItem.h"

#include "ShapePathBuilder.h"

HexagramItem::HexagramItem()
{
    setContentSize(defaultContentSize());
}

QSizeF HexagramItem::defaultContentSize() const
{
    return QSizeF(128.0, 112.0);
}

QPainterPath HexagramItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::hexagramPath(rect);
}
