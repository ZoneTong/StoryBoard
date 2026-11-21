#pragma once

#include "ShapeItem.h"

// Heart：轮廓由 ShapePathBuilder 按 ShapeType::HEART 生成，没有额外参数。
class HeartItem : public ShapeItem {
  public:
    HeartItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::HEART; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
