#include "items/shapes/FlagWaveItem.h"

#include "items/geometry/ShapePathBuilder.h"

FlagWaveItem::FlagWaveItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF FlagWaveItem::DefaultContentSize() const
{
    return QSizeF(128.0, 86.0);
}

QPainterPath FlagWaveItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::FlagWavePath(rect);
}
