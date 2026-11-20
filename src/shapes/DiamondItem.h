#pragma once

#include "ShapeItem.h"

// Diamond：轮廓由 ShapePathBuilder 按 ShapeType::Diamond 生成，没有额外参数。
class DiamondItem : public ShapeItem {
  public:
    DiamondItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::Diamond; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
