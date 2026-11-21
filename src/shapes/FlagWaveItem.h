#pragma once

#include "ShapeItem.h"

// FlagWave：轮廓由 ShapePathBuilder 按 ShapeType::FLAG_WAVE 生成，没有额外参数。
class FlagWaveItem : public ShapeItem {
  public:
    FlagWaveItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::FLAG_WAVE; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
