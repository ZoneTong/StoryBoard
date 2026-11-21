#pragma once

#include "ShapeItem.h"

// Scroll：轮廓由 ShapePathBuilder 按 ShapeType::SCROLL 生成，没有额外参数。
class ScrollItem : public ShapeItem {
  public:
    ScrollItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::SCROLL; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
