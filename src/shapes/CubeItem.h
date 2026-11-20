#pragma once

#include "ShapeItem.h"

// Cube：轮廓由 ShapePathBuilder 按 ShapeType::Cube 生成，没有额外参数。
class CubeItem : public ShapeItem {
  public:
    CubeItem();
    ShapeType shapeType() const override { return ShapeType::Cube; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
