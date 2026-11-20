#pragma once

#include "ShapeItem.h"

// Ellipse：轮廓由 ShapePathBuilder 按 ShapeType::Ellipse 生成，没有额外参数。
class EllipseItem : public ShapeItem {
  public:
    EllipseItem();
    ShapeType shapeType() const override { return ShapeType::Ellipse; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
