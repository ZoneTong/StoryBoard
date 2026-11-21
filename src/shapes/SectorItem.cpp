#include "shapes/SectorItem.h"

SectorItem::SectorItem()
{
    SetContentSize(defaultContentSize());
    setFrame(ContentRect());
}

void SectorItem::ContentResized()
{
    ShapeItem::ContentResized();
    // 外接框与内容矩形保持一致，扇形只由 EllipseShape::sector() 出。
    setFrame(ContentRect());
}

ItemPropertySet SectorItem::Properties() const
{
    ItemPropertySet result = ShapeItem::Properties();
    result.insert(ItemProperty::START_ANGLE);
    result.insert(ItemProperty::SWEEP_ANGLE);
    return result;
}

QPainterPath SectorItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return sector(StartAngle(), SweepAngle());
}
