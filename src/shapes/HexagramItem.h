#pragma once

#include "ShapeItem.h"

// Hexagram：轮廓由 ShapePathBuilder 按 ShapeType::Hexagram 生成，没有额外参数。
class HexagramItem : public ShapeItem {
  public:
    HexagramItem();
    ShapeType shapeType() const override { return ShapeType::Hexagram; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
