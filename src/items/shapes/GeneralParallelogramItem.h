#pragma once

#include "items/shapes/ParallelogramAbstractItem.h"

// 普通平行四边形叶子：左下内角（度）可编辑，上边水平偏移由高度与角度派生。
// 左下内角用一个贴着该顶点的旋钮拖动：旋钮绕顶点转多少度，内角就变多少度，
// 拖过 90° 连续（锐角↔钝角），不会在 90° 处断档。
class GeneralParallelogramItem : public ParallelogramAbstractItem {
  public:
    GeneralParallelogramItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::PARALLELOGRAM; }
    ItemPropertySet Properties() const override;
    // 左下内角旋钮：贴着左下顶点，绕顶点拖动连续改变内角（锐角↔钝角）。
    HandleSpecList CustomHandles() const override;
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;
    void SetParallelogramAngle(qreal angle) override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;

  private:
    // 拖旋钮期间的锚点：固定不动的左下顶点（场景坐标）与起手时的角度 / 指针朝向。
    QPointF m_skewDragPivot;
    qreal m_skewDragStartAngle = 0.0;
    qreal m_skewDragStartPointer = 0.0;
};
