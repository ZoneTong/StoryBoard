#include "shapes/EllipseItem.h"

EllipseItem::EllipseItem()
{
    SetContentSize(defaultContentSize());
    setFrame(ContentRect());
}

void EllipseItem::ContentResized()
{
    ShapeItem::ContentResized();
    // 外接框与内容矩形保持一致，轮廓只由 EllipseShape::ellipse() 出。
    setFrame(ContentRect());
}

QPainterPath EllipseItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return ellipse();
}
