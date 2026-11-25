#pragma once

#include "items/EllipseBase.h"
#include "items/shapes/ShapeAbstractItem.h"

// 扇形：椭圆外接框即内容矩形，起止角可编辑；选中框贴在轮廓的最小覆盖矩形上。
// 起止角、外接框、端点手柄与读数来自 EllipseBase；轮廓只由 EllipseGeometry::Sector() 出。
class SectorItem : public ShapeAbstractItem, public EllipseBase {
  public:
    SectorItem();
    ShapeType shapeType() const override { return ShapeType::SECTOR; }
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    ShapeItemState State() const override;
    void ApplyState(const ShapeItemState& state) override;
    // §3.3：弧边两端各一个端点手柄（CUSTOM_0 起始端、CUSTOM_1 结束端），
    // 拖起始端固定结束端，反之亦然；只改 start / sweep。
    HandleSpecList CustomHandles() const override;
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;
    void EndHandleDrag() override;

  protected:
    void ContentResized() override;
    void DrawSelectionOverlay(QPainter* painter) override;
    QList<QRectF> AngleReadoutRects() const override;
    QPainterPath ShapePath(const QRectF& rect) const override;
    void ArcAnglesChanged() override;
    void WriteJsonExtra(QJsonObject& json) const override;
    void ReadJsonExtra(const QJsonObject& json) override;
};
