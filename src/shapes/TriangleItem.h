#pragma once

#include "ShapeAbstractItem.h"

// 三角形：底边两侧的底角是真相，高度由底边与角度派生；顶点句柄与底边刻度线
// 也由本类给出（见 ShapeAbstractItem 的 Triangle 分支）。
class TriangleItem : public ShapeAbstractItem {
  public:
    TriangleItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::TRIANGLE; }
    ItemPropertySet Properties() const override;
    // 顶点 C 的内角手柄：取代顶边中点手柄，拖它只改底角 A（§3.1）。
    HandleSpecList CustomHandles() const override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
