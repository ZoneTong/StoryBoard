#include "shapes/TriangleItem.h"

#include "ShapePathBuilder.h"

TriangleItem::TriangleItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF TriangleItem::DefaultContentSize() const
{
    return QSizeF(128.0, 111.0);
}

ItemPropertySet TriangleItem::Properties() const
{
    ItemPropertySet result = ShapeItem::Properties();
    result.Insert(ItemProperty::TRIANGLE_BASE);
    result.Insert(ItemProperty::BOTTOM_ANGLE1);
    result.Insert(ItemProperty::BOTTOM_ANGLE2);
    return result;
}

QPainterPath TriangleItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::TrianglePath(rect, BottomAngle1(), BottomAngle2());
}
