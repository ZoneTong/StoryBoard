#pragma once

#include "ShapeItem.h"

// Cylinder：轮廓由 ShapePathBuilder 按 ShapeType::Cylinder 生成，没有额外参数。
class CylinderItem : public ShapeItem {
  public:
    CylinderItem();
    ShapeType shapeType() const override { return ShapeType::Cylinder; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
