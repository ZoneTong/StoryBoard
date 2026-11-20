#include "shapes/SectorItem.h"

#include "ShapePathBuilder.h"

SectorItem::SectorItem()
{
    setContentSize(defaultShapeSize(ShapeType::Sector));
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
    return ShapePathBuilder::pathFor(ShapeType::Sector, rect, -1.0, startAngle(), sweepAngle());
}
