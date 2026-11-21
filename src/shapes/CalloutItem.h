#pragma once

#include "ShapeItem.h"

// Callout：轮廓由 ShapePathBuilder 按 ShapeType::CALLOUT 生成，没有额外参数。
class CalloutItem : public ShapeItem {
  public:
    CalloutItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::CALLOUT; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
