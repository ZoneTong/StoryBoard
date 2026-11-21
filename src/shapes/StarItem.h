#pragma once

#include "ShapeItem.h"

// Star：轮廓由 ShapePathBuilder 按 ShapeType::STAR 生成，没有额外参数。
class StarItem : public ShapeItem {
  public:
    StarItem();
    ShapeType shapeType() const override { return ShapeType::STAR; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
