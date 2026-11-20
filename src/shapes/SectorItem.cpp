#include "shapes/SectorItem.h"

SectorItem::SectorItem()
{
    setContentSize(defaultContentSize());
    setFrame(contentRect());
}

void SectorItem::contentResized()
{
    ShapeItem::contentResized();
    // 外接框与内容矩形保持一致，扇形只由 EllipseShape::sector() 出。
    setFrame(contentRect());
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
    Q_UNUSED(rect)
    return sector(startAngle(), sweepAngle());
}
