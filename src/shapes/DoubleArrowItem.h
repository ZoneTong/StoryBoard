#pragma once

#include "ShapeItem.h"

// DoubleArrow：轮廓由 ShapePathBuilder 按 ShapeType::DOUBLE_ARROW 生成，没有额外参数。
class DoubleArrowItem : public ShapeItem {
  public:
    DoubleArrowItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::DOUBLE_ARROW; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
