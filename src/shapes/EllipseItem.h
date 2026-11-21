#pragma once

#include "EllipseShape.h"
#include "ShapeItem.h"

// 椭圆：第二基类 EllipseShape 提供几何（外接框即内容矩形），没有额外参数。
class EllipseItem : public ShapeItem, public EllipseShape {
  public:
    EllipseItem();
    ShapeType shapeType() const override { return ShapeType::ELLIPSE; }

  protected:
    void contentResized() override;
    QPainterPath shapePath(const QRectF& rect) const override;
};
