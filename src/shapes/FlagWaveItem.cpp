#include "shapes/FlagWaveItem.h"

#include "ShapePathBuilder.h"

FlagWaveItem::FlagWaveItem()
{
    setContentSize(defaultShapeSize(ShapeType::FlagWave));
}

QPainterPath FlagWaveItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::FlagWave, rect, -1.0);
}
