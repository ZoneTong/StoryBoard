#pragma once

#include "ShapeAbstractItem.h"

// Callout：轮廓由 ShapePathBuilder 按 ShapeType::CALLOUT 生成，没有额外参数。
class CalloutItem : public ShapeAbstractItem {
  public:
    CalloutItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::CALLOUT; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
