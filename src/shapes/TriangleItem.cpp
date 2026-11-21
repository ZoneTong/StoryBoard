#include "shapes/TriangleItem.h"

#include "ShapePathBuilder.h"

TriangleItem::TriangleItem()
{
    SetContentSize(defaultContentSize());
}

QSizeF TriangleItem::defaultContentSize() const
{
    return QSizeF(128.0, 111.0);
}

ItemPropertySet TriangleItem::Properties() const
{
    ItemPropertySet result = ShapeItem::Properties();
    result.insert(ItemProperty::TRIANGLE_BASE);
    result.insert(ItemProperty::BOTTOM_ANGLE1);
    result.insert(ItemProperty::BOTTOM_ANGLE2);
    return result;
}

QPainterPath TriangleItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::trianglePath(rect, BottomAngle1(), BottomAngle2());
}
