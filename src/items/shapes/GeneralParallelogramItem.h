#pragma once

#include "items/shapes/ParallelogramAbstractItem.h"

// 普通平行四边形叶子：左下内角（度）可编辑，上边水平偏移由高度与角度派生。
// 内角用手柄拖拽比“拖顶点”更稳：上边中点旋钮的有符号偏移直接对应锐角/钝角，
// 拖过中心即从锐角连续变到钝角，不会在 90° 处断档。
class GeneralParallelogramItem : public ParallelogramAbstractItem {
  public:
    GeneralParallelogramItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::PARALLELOGRAM; }
    ItemPropertySet Properties() const override;
    // 顶边中点下方的内角倾斜旋钮：水平拖动连续改变内角（锐角↔钝角）。
    HandleSpecList CustomHandles() const override;
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;
    void SetParallelogramAngle(qreal angle) override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;

  private:
    // 拖旋钮期间固定的底边锚点（场景坐标）与底边长度。
    QPointF m_skewDragBottomLeft;
    qreal m_skewDragBaseLength = 0.0;
};
