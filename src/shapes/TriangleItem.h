#pragma once

#include "ShapeItem.h"

// 三角形：底边两侧的底角是真相，高度由底边与角度派生；顶点句柄与底边刻度线
// 也由本类给出（见 ShapeItem 的 Triangle 分支）。
class TriangleItem : public ShapeItem {
  public:
    TriangleItem();
    ShapeType shapeType() const override { return ShapeType::Triangle; }
    ItemPropertySet properties() const override;
    bool apexAngleHandle() const override { return true; }

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
