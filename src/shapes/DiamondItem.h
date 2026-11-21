#pragma once

#include "shapes/ParallelogramItem.h"

// 菱形：平行四边形家族的四边恒等特例。轮廓取内容框四边中点，两条对角线天然
// 互相垂直平分，因此任意宽高下四条边永远等长；插入时旋转为 0，一条对角线水平。
// 内角由宽高比决定，不再单独开放“内角”属性与倾斜旋钮。
class DiamondItem : public ParallelogramItem {
  public:
    DiamondItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::DIAMOND; }
    ItemPropertySet Properties() const override;
    bool ApexAngleHandle() const override { return false; }
    bool SkewHandleEnabled() const override { return false; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
