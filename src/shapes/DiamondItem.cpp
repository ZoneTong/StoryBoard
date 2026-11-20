#include "shapes/DiamondItem.h"

#include "ShapePathBuilder.h"

DiamondItem::DiamondItem()
{
    setContentSize(defaultContentSize());
}

QSizeF DiamondItem::defaultContentSize() const
{
    return QSizeF(128.0, 100.0);
}

QPainterPath DiamondItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::diamondPath(rect);
}
