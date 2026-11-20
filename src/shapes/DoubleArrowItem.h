#pragma once

#include "ShapeItem.h"

// DoubleArrow：轮廓由 ShapePathBuilder 按 ShapeType::DoubleArrow 生成，没有额外参数。
class DoubleArrowItem : public ShapeItem {
  public:
    DoubleArrowItem();
    ShapeType shapeType() const override { return ShapeType::DoubleArrow; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
