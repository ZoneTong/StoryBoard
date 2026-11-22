#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// Hexagon：平顶正六边形（§3.5），轮廓由 ShapePathBuilder::HexagonPath 生成，没有额外参数。
class HexagonItem : public ShapeAbstractItem {
  public:
    HexagonItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::HEXAGON; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
