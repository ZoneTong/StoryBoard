#include "shapes/TriangleItem.h"

#include "ShapePathBuilder.h"

TriangleItem::TriangleItem()
{
    setContentSize(defaultShapeSize(ShapeType::Triangle));
}

ItemPropertySet TriangleItem::properties() const
{
    ItemPropertySet result = ShapeItem::properties();
    result.insert(ItemProperty::TRIANGLE_BASE);
    result.insert(ItemProperty::BOTTOM_ANGLE1);
    result.insert(ItemProperty::BOTTOM_ANGLE2);
    return result;
}

QPainterPath TriangleItem::shapePath(const QRectF& rect) const
{
    return ShapePathBuilder::trianglePath(rect, bottomAngle1(), bottomAngle2());
}
