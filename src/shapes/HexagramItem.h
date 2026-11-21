#pragma once

#include "ShapeItem.h"

// Hexagram：轮廓由 ShapePathBuilder 按 ShapeType::HEXAGRAM 生成，没有额外参数。
class HexagramItem : public ShapeItem {
  public:
    HexagramItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::HEXAGRAM; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
