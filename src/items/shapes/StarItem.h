#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// Star：轮廓由 ShapePathBuilder 按 ShapeType::STAR 生成，没有额外参数。
class StarItem : public ShapeAbstractItem {
  public:
    StarItem();
    ShapeType shapeType() const override { return ShapeType::STAR; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
