#pragma once

#include "ShapeItem.h"

// 扇形 / 弧线：椭圆外接框即内容矩形，起止角可编辑；选中框贴在轮廓的最小覆盖矩形上。
class SectorItem : public ShapeItem {
  public:
    SectorItem();
    ShapeType shapeType() const override { return ShapeType::Sector; }
    ItemPropertySet properties() const override;

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
