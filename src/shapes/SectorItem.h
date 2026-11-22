#pragma once

#include "ArcAngleAttributes.h"
#include "EllipseShape.h"
#include "ShapeAbstractItem.h"

// 扇形：椭圆外接框即内容矩形，起止角可编辑；选中框贴在轮廓的最小覆盖矩形上。
// 弧度参数属于本类自己：基类完全不再持有弧角字段，椭圆几何仍由 EllipseShape 提供。
class SectorItem : public ShapeAbstractItem, public EllipseShape, public ArcAngleAttributes {
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

  protected:
    void ContentResized() override;
    QPainterPath ShapePath(const QRectF& rect) const override;
    void ArcAnglesChanged() override;
    void WriteJsonExtra(QJsonObject& json) const override;
    void ReadJsonExtra(const QJsonObject& json) override;
};
