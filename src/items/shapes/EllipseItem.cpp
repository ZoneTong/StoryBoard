#include "items/shapes/EllipseItem.h"

#include "items/geometry/EllipseAxis.h"

EllipseItem::EllipseItem()
{
    SetContentSize(DefaultContentSize());
    SetFrame(ContentRect());
}

ItemPropertySet EllipseItem::Properties() const
{
    // 内容框即椭圆外接框：长短半轴由尺寸派生，写回时反向换算。
    ItemPropertySet properties = ShapeAbstractItem::Properties();
    if (!IsCustomShape()) {
        properties.Insert(ItemProperty::MAJOR_AXIS);
        properties.Insert(ItemProperty::MINOR_AXIS);
    }
    return properties;
}

QVariant EllipseItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::MAJOR_AXIS:
        return EllipseAxis::Major(ContentSize());
    case ItemProperty::MINOR_AXIS:
        return EllipseAxis::Minor(ContentSize());
    default:
        return ShapeAbstractItem::PropertyValue(property);
    }
}

void EllipseItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::MAJOR_AXIS:
    case ItemProperty::MINOR_AXIS: {
        const EllipseAxis::ResizeResult resize =
            EllipseAxis::Apply(property == ItemProperty::MAJOR_AXIS, value.toDouble(), pos(), ContentSize());
        setPos(resize.pos);
        SetContentSize(resize.size);
        break;
    }
    default:
        ShapeAbstractItem::SetPropertyValue(property, value);
        break;
    }
}

void EllipseItem::ContentResized()
{
    ShapeAbstractItem::ContentResized();
    // 外接框与内容矩形保持一致，轮廓只由 EllipseGeometry::Ellipse() 出。
    SetFrame(ContentRect());
}

QPainterPath EllipseItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return Ellipse();
}
