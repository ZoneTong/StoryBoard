#pragma once

#include "shapes/ParallelogramAbstractItem.h"

// 菱形：平行四边形族的四边恒等特例。轮廓取内容框四边中点，两条对角线天然
// 互相垂直平分，因此任意宽高下四条边永远等长；插入时旋转为 0，一条对角线水平。
// 复用内角 A：尺寸是落盘真相，内角由宽高派生，不写 parallelogramAngle。
class DiamondItem : public ParallelogramAbstractItem {
  public:
    DiamondItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::DIAMOND; }
    ItemPropertySet Properties() const override;
    // 左/右顶点角：tan(A/2) = h/w，随宽高连续变化。
    qreal ParallelogramAngle() const override;
    // 写入侧按 h = w·tan(A/2) 由宽反算高，中心不动。
    void SetParallelogramAngle(qreal angle) override;

  protected:
    // 角度浮层贴左顶点：拖外接框 / 边时显示，不新增角度句柄。
    QPointF AngleReadoutAnchor() const override;
    QPainterPath ShapePath(const QRectF& rect) const override;
};
