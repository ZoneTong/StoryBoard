#include "shapes/HexagramItem.h"

#include "ShapePathBuilder.h"

HexagramItem::HexagramItem()
{
    SetContentSize(defaultContentSize());
}

QSizeF HexagramItem::defaultContentSize() const
{
    return QSizeF(128.0, 112.0);
}

QPainterPath HexagramItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::hexagramPath(rect);
}
