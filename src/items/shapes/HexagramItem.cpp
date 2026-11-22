#include "items/shapes/HexagramItem.h"

#include "items/geometry/ShapePathBuilder.h"

HexagramItem::HexagramItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF HexagramItem::DefaultContentSize() const
{
    return QSizeF(128.0, 112.0);
}

QPainterPath HexagramItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::HexagramPath(rect);
}
