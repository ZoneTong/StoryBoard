#pragma once

#include "ShapeItem.h"

// Arrow：轮廓由 ShapePathBuilder 按 ShapeType::ARROW 生成，没有额外参数。
class ArrowItem : public ShapeItem {
  public:
    ArrowItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::ARROW; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
