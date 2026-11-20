#pragma once

#include "ShapeItem.h"

// FlagWave：轮廓由 ShapePathBuilder 按 ShapeType::FlagWave 生成，没有额外参数。
class FlagWaveItem : public ShapeItem {
  public:
    FlagWaveItem();
    ShapeType shapeType() const override { return ShapeType::FlagWave; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
