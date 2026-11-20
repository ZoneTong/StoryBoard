#pragma once

#include "ShapeItem.h"

// 平行四边形：左下内角（度）可编辑，上边水平偏移由高度与角度派生。
class ParallelogramItem : public ShapeItem {
  public:
    ParallelogramItem();
    ShapeType shapeType() const override { return ShapeType::Parallelogram; }
    ItemPropertySet properties() const override;
    bool apexAngleHandle() const override { return true; }
    void setParallelogramAngle(qreal angle) override;
    QPointF customHandleCenter(int handle, bool* ok) const override;

  protected:
    QPainterPath shapePath(const QRectF& rect) const override;
    void drawSelectionOverlay(QPainter* painter) override;
};
