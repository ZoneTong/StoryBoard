#pragma once

#include "ShapeAbstractItem.h"

// Cylinder：轮廓由 ShapePathBuilder 按 ShapeType::CYLINDER 生成，没有额外参数。
class CylinderItem : public ShapeAbstractItem {
  public:
    CylinderItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::CYLINDER; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
