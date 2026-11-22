#include "shapes/SectorItem.h"

SectorItem::SectorItem()
{
    SetContentSize(DefaultContentSize());
    SetFrame(ContentRect());
}

void SectorItem::ContentResized()
{
    ShapeAbstractItem::ContentResized();
    // 外接框与内容矩形保持一致，扇形只由 EllipseShape::Sector() 出。
    SetFrame(ContentRect());
}

ItemPropertySet SectorItem::Properties() const
{
    return ShapeAbstractItem::Properties().United(ArcAngleAttributes::ArcAngleProperties());
}

QPainterPath SectorItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return Sector(StartAngle(), SweepAngle());
}
