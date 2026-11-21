#include "shapes/EllipseItem.h"

EllipseItem::EllipseItem()
{
    SetContentSize(DefaultContentSize());
    SetFrame(ContentRect());
}

void EllipseItem::ContentResized()
{
    ShapeItem::ContentResized();
    // 外接框与内容矩形保持一致，轮廓只由 EllipseShape::Ellipse() 出。
    SetFrame(ContentRect());
}

QPainterPath EllipseItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return Ellipse();
}
