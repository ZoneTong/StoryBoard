#pragma once

#include "ShapeItem.h"

// Cube：轮廓由 ShapePathBuilder 按 ShapeType::CUBE 生成，没有额外参数。
class CubeItem : public ShapeItem {
  public:
    CubeItem();
    ShapeType shapeType() const override { return ShapeType::CUBE; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
