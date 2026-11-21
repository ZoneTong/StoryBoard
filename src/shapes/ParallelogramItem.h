#pragma once

#include "ShapeItem.h"

// 平行四边形：左下内角（度）可编辑，上边水平偏移由高度与角度派生。
class ParallelogramItem : public ShapeItem {
  public:
    ParallelogramItem();
    QSizeF defaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::PARALLELOGRAM; }
    ItemPropertySet Properties() const override;
    bool ApexAngleHandle() const override { return true; }
    void SetParallelogramAngle(qreal angle) override;
    QPointF CustomHandleCenter(int handle, bool* ok) const override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
    void DrawSelectionOverlay(QPainter* painter) override;
};
