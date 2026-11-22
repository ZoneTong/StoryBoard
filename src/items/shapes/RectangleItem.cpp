#include "items/shapes/RectangleItem.h"

#include "items/geometry/ShapePathBuilder.h"

RectangleItem::RectangleItem()
{
    SetContentSize(DefaultContentSize());
    SetParallelogramAngle(90.0);
}

ItemPropertySet RectangleItem::Properties() const
{
    // 矩形没有内角行（锁 90°），只有圆角行。
    ItemPropertySet result = ShapeAbstractItem::Properties();
    result.Insert(ItemProperty::CORNER_RADIUS);
    return result;
}

void RectangleItem::ReadJsonExtra(const QJsonObject& json)
{
    ParallelogramAbstractItem::ReadJsonExtra(json);
    SetParallelogramAngle(90.0);
}

void RectangleItem::SetParallelogramAngle(qreal angle)
{
    Q_UNUSED(angle);
    ParallelogramAbstractItem::SetParallelogramAngle(90.0);
}

QPainterPath RectangleItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::RoundedRectPath(rect, CornerRadius());
}
