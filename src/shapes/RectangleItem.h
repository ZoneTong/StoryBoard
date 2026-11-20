#pragma once

#include "shapes/ParallelogramItem.h"

// 矩形：平行四边形的特例（内角恒 90°），圆角只属于这一种图形。
// 圆角半径按画布 px 存，0 表示直角。
class RectangleItem : public ParallelogramItem {
  public:
    RectangleItem();
    ShapeType shapeType() const override { return ShapeType::Rectangle; }
    ItemPropertySet properties() const override;
    // 矩形锁 90°：写入侧统一抹平，面板也没有内角行。
    void setParallelogramAngle(qreal angle) override;
    bool apexAngleHandle() const override { return false; }
    QPointF customHandleCenter(int handle, bool* ok) const override;

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
};
