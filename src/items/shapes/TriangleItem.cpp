#include "items/shapes/TriangleItem.h"

#include "items/geometry/ShapePathBuilder.h"

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
    ItemPropertySet result = ShapeAbstractItem::Properties();
    result.Insert(ItemProperty::BOTTOM_ANGLE1);
    result.Insert(ItemProperty::BOTTOM_ANGLE2);
    return result;
}

QPainterPath TriangleItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::TrianglePath(rect, BottomAngle1(), BottomAngle2());
}

HandleSpecList TriangleItem::CustomHandles() const
{
    HandleSpecList specs;
    if (IsCustomShape()) {
        return specs;
    }
    // 顶点 C 的手柄向下偏移，让开底角 90° 时与角顶点重合的角落手柄；
    // 形很矮时收在内框里。位置随视图缩放换算，保证屏幕上尺寸稳定。
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    const qreal inset = qMin<qreal>(18.0 / scale, qMax<qreal>(0.0, ContentRect().height() - 7.0 / scale));
    HandleSpec apex;
    apex.id = CanvasItem::CUSTOM_0;
    apex.position = triangleVertex(TriangleVertex::C) + QPointF(0.0, inset);
    apex.behavior = HandleBehavior::ANGLE;
    apex.cursor = Qt::SizeAllCursor;
    apex.replacesTopHandle = true;
    specs.append(apex);
    return specs;
}
