#pragma once

#include "ShapeItem.h"

// Scroll：轮廓由 ShapePathBuilder 按 ShapeType::Scroll 生成，没有额外参数。
class ScrollItem : public ShapeItem {
  public:
    ScrollItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::Scroll; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
