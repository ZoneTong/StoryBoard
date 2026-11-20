#pragma once

#include "ShapeItem.h"

// Star：轮廓由 ShapePathBuilder 按 ShapeType::Star 生成，没有额外参数。
class StarItem : public ShapeItem {
  public:
    StarItem();
    ShapeType shapeType() const override { return ShapeType::Star; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
