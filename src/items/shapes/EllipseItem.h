#pragma once

#include "items/geometry/EllipseGeometry.h"
#include "items/shapes/ShapeAbstractItem.h"

// 椭圆：第二基类 EllipseGeometry 提供几何（外接框即内容矩形），没有额外参数。
class EllipseItem : public ShapeAbstractItem, public EllipseGeometry {
  public:
    EllipseItem();
    ShapeType shapeType() const override { return ShapeType::ELLIPSE; }
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;

  protected:
    void ContentResized() override;
    QPainterPath ShapePath(const QRectF& rect) const override;
};
