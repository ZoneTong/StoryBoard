#include "shapes/RectangleItem.h"

#include "ShapePathBuilder.h"

RectangleItem::RectangleItem()
{
    SetContentSize(DefaultContentSize());
    SetParallelogramAngle(90.0);
}

ItemPropertySet RectangleItem::Properties() const
{
    // 不走 ParallelogramItem::properties：矩形没有内角行，只有圆角行。
    ItemPropertySet result = ShapeItem::Properties();
    result.Insert(ItemProperty::CORNER_RADIUS);
    return result;
}

void RectangleItem::ReadJsonExtra(const QJsonObject& json)
{
    ParallelogramItem::ReadJsonExtra(json);
    SetParallelogramAngle(90.0);
}

void RectangleItem::SetParallelogramAngle(qreal angle)
{
    Q_UNUSED(angle);
    ShapeItem::SetParallelogramAngle(90.0);
}

QPointF RectangleItem::CustomHandleCenter(int handle, bool* ok) const
{
    Q_UNUSED(handle);
    if (ok) {
        *ok = false;
    }
    return QPointF();
}

QPainterPath RectangleItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::RoundedRectPath(rect, CornerRadius());
}
