#pragma once

#include "ShapeAbstractItem.h"

// 平行四边形：左下内角（度）可编辑，上边水平偏移由高度与角度派生。
// 内角用手柄拖拽比“拖顶点”更稳：上边中点旋钮的有符号偏移直接对应锐角/钝角，
// 拖过中心即从锐角连续变到钝角，不会在 90° 处断档。
class ParallelogramItem : public ShapeAbstractItem {
  public:
    ParallelogramItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::PARALLELOGRAM; }
    ItemPropertySet Properties() const override;
    bool ApexAngleHandle() const override { return false; }
    bool SkewHandleEnabled() const override { return true; }
    void SetParallelogramAngle(qreal angle) override;
    QPointF CustomHandleCenter(int handle, bool* ok) const override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
    void DrawSelectionOverlay(QPainter* painter) override;
};
