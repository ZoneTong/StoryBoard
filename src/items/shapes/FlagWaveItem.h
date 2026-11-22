#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// FlagWave：轮廓由 ShapePathBuilder 按 ShapeType::FLAG_WAVE 生成，没有额外参数。
class FlagWaveItem : public ShapeAbstractItem {
  public:
    FlagWaveItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::FLAG_WAVE; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
