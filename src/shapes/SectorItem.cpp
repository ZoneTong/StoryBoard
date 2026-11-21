#include "shapes/SectorItem.h"

SectorItem::SectorItem()
{
    SetContentSize(DefaultContentSize());
    SetFrame(ContentRect());
}

void SectorItem::ContentResized()
{
    ShapeItem::ContentResized();
    // 外接框与内容矩形保持一致，扇形只由 EllipseShape::Sector() 出。
    SetFrame(ContentRect());
}

ItemPropertySet SectorItem::Properties() const
{
    ItemPropertySet result = ShapeItem::Properties();
    result.Insert(ItemProperty::START_ANGLE);
    result.Insert(ItemProperty::SWEEP_ANGLE);
    return result;
}

QPainterPath SectorItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return Sector(StartAngle(), SweepAngle());
}
