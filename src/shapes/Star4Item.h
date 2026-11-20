#pragma once

#include "ShapeItem.h"

// Star4：轮廓由 ShapePathBuilder 按 ShapeType::Star4 生成，没有额外参数。
class Star4Item : public ShapeItem {
  public:
    Star4Item();
    ShapeType shapeType() const override { return ShapeType::Star4; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
