#pragma once

#include "ShapeItem.h"

// Arrow：轮廓由 ShapePathBuilder 按 ShapeType::Arrow 生成，没有额外参数。
class ArrowItem : public ShapeItem {
  public:
    ArrowItem();
    ShapeType shapeType() const override { return ShapeType::Arrow; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
