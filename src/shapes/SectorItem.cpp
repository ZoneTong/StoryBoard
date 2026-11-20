#include "shapes/SectorItem.h"

#include "ShapePathBuilder.h"

SectorItem::SectorItem()
{
    setContentSize(defaultContentSize());
}

ItemPropertySet SectorItem::properties() const
{
    ItemPropertySet result = ShapeItem::properties();
    result.insert(ItemProperty::START_ANGLE);
    result.insert(ItemProperty::SWEEP_ANGLE);
    return result;
}

QPainterPath SectorItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::sectorPath(rect, startAngle(), sweepAngle());
}
