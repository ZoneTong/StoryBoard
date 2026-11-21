#pragma once

#include "ShapeItem.h"

// 三角形：底边两侧的底角是真相，高度由底边与角度派生；顶点句柄与底边刻度线
// 也由本类给出（见 ShapeItem 的 Triangle 分支）。
class TriangleItem : public ShapeItem {
  public:
    TriangleItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::TRIANGLE; }
    ItemPropertySet Properties() const override;
    bool ApexAngleHandle() const override { return true; }

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
};
