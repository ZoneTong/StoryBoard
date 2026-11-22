#include "shapes/DiamondItem.h"

#include "ShapePathBuilder.h"

DiamondItem::DiamondItem()
{
    SetContentSize(DefaultContentSize());
    // 四边恒等的菱形内角由宽高比派生；这里固定 90°，只作为继承来的状态占位，
    // 不进入属性面板，也不写出 parallelogramAngle。
    SetParallelogramAngle(90.0);
}

QSizeF DiamondItem::DefaultContentSize() const
{
    // 横对角线 128、竖对角线 72：插入即“一条对角线水平”，与 Excalidraw 的
    // diamond 外接框语义一致（宽即水平对角线）。
    return QSizeF(128.0, 72.0);
}

ItemPropertySet DiamondItem::Properties() const
{
    // 菱形不开放内角：shapeType 决定轮廓，宽高决定两条对角线。
    return ShapeAbstractItem::Properties();
}

QPainterPath DiamondItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::DiamondPath(rect);
}
