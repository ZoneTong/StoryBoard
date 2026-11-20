#include "shapes/FlagWaveItem.h"

#include "ShapePathBuilder.h"

FlagWaveItem::FlagWaveItem()
{
    setContentSize(defaultContentSize());
}

QSizeF FlagWaveItem::defaultContentSize() const
{
    return QSizeF(128.0, 86.0);
}

QPainterPath FlagWaveItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::flagWavePath(rect);
}
