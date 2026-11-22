#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// Star4：轮廓由 ShapePathBuilder 按 ShapeType::STAR4 生成，没有额外参数。
class Star4Item : public ShapeAbstractItem {
  public:
    Star4Item();
    ShapeType shapeType() const override { return ShapeType::STAR4; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
