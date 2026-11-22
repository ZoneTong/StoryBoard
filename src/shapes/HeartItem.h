#pragma once

#include "ShapeAbstractItem.h"

// Heart：轮廓由 ShapePathBuilder 按 ShapeType::HEART 生成，没有额外参数。
class HeartItem : public ShapeAbstractItem {
  public:
    HeartItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::HEART; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
