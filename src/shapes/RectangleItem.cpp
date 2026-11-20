#include "shapes/RectangleItem.h"

#include "ShapePathBuilder.h"

RectangleItem::RectangleItem()
{
    setContentSize(defaultShapeSize(ShapeType::Rectangle));
    setParallelogramAngle(90.0);
}

ItemPropertySet RectangleItem::properties() const
{
    // 不走 ParallelogramItem::properties：矩形没有内角行，只有圆角行。
    ItemPropertySet result = ShapeItem::properties();
    result.insert(ItemProperty::CORNER_RADIUS);
    return result;
}

void RectangleItem::setParallelogramAngle(qreal angle)
{
    Q_UNUSED(angle);
    ShapeItem::setParallelogramAngle(90.0);
}

QPointF RectangleItem::customHandleCenter(int handle, bool* ok) const
{
    Q_UNUSED(handle);
    if (ok) {
        *ok = false;
    }
    return QPointF();
}

QPainterPath RectangleItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::pathFor(ShapeType::Rectangle, rect, cornerRadius());
}
