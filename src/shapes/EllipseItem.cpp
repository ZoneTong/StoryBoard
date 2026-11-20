#include "shapes/EllipseItem.h"

EllipseItem::EllipseItem()
{
    setContentSize(defaultContentSize());
    setFrame(contentRect());
}

void EllipseItem::contentResized()
{
    ShapeItem::contentResized();
    // 外接框与内容矩形保持一致，轮廓只由 EllipseShape::ellipse() 出。
    setFrame(contentRect());
}

QPainterPath EllipseItem::shapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return ellipse();
}
