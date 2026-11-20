#pragma once

#include "ShapeItem.h"

// Heart：轮廓由 ShapePathBuilder 按 ShapeType::Heart 生成，没有额外参数。
class HeartItem : public ShapeItem {
  public:
    HeartItem();
    ShapeType shapeType() const override { return ShapeType::Heart; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
