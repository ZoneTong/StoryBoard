#pragma once

#include "ArcAngleAttributes.h"
#include "EllipseShape.h"
#include "I18n.h"
#include "LineItemAbstract.h"

#include <QRectF>

// 弧线图元：同时派生自描线图元（零面积描线）、角度属性基类（起止角）与椭圆几何（角度语义、采样）。
// 椭圆外接框即插入时给出的矩形；角度是几何真相，改角度按上次相对框重新采样。
class ArcItem : public LineItemAbstract, public ArcAngleAttributes, public EllipseShape {
    Q_OBJECT
  public:
    explicit ArcItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("arc"); }
    QString DisplayName() const override;
    QRectF FrameRect() const override;
    // 弧线所在椭圆的真实外接尺寸（不是弧线笔迹的包围盒），供属性窗显示长短半轴。
    QSizeF EllipseSize() const;

    void SetAngles(qreal startDeg, qreal sweepDeg);
    void SetArcAngles(qreal startDeg, qreal sweepDeg) override;
    // 长短半轴写入：保持椭圆中心不动，按当前长轴方向换算外接框后重建描线几何。
    void SetArcAxes(qreal majorAxis, qreal minorAxis);

    // 插入入口：椭圆框 + 起始角 / 扫过角。
    void SetArcGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                        const QColor& color, qreal width);

    bool SupportsArcAngles() const override { return true; }
    ParamState paramState() const override;
    void ApplyParamState(const ParamState& state) override;
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;

    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;

  protected:
    void ArcAnglesChanged() override;

  private:
    // 角度变化后按“上次椭圆框相对内容框的比例”重建，拉伸过再改角度不会跳。
    void Rebuild();
    // 以给定场景坐标椭圆框重建：写入半轴与角度重建共用这一份采样与 refFrame 回写。
    void RebuildWithFrame(const QRectF& frameScene);

    QRectF m_refFrame{0.0, 0.0, 1.0, 1.0};
};
