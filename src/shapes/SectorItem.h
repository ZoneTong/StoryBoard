#pragma once

#include "EllipseShape.h"
#include "ShapeItem.h"

// 扇形：椭圆外接框即内容矩形，起止角可编辑；选中框贴在轮廓的最小覆盖矩形上。
// 第二基类 EllipseShape 提供角度语义与扇形几何，与椭圆 / 弧线共享一份实现。
class SectorItem : public ShapeItem, public EllipseShape {
  public:
    SectorItem();
    ShapeType shapeType() const override { return ShapeType::SECTOR; }
    ItemPropertySet properties() const override;

  protected:
    void contentResized() override;
    QPainterPath shapePath(const QRectF& rect) const override;
};
