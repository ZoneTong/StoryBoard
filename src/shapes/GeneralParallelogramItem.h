#pragma once

#include "shapes/ParallelogramAbstractItem.h"

// 普通平行四边形叶子：左下内角（度）可编辑，上边水平偏移由高度与角度派生。
// 内角用手柄拖拽比“拖顶点”更稳：上边中点旋钮的有符号偏移直接对应锐角/钝角，
// 拖过中心即从锐角连续变到钝角，不会在 90° 处断档。
class GeneralParallelogramItem : public ParallelogramAbstractItem {
  public:
    GeneralParallelogramItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::PARALLELOGRAM; }
    ItemPropertySet Properties() const override;
    bool SkewHandleEnabled() const override { return true; }
    void SetParallelogramAngle(qreal angle) override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
