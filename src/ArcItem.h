#pragma once

#include "EllipseShape.h"
#include "I18n.h"
#include "LineItemAbstract.h"

#include <QRectF>

// 弧线图元：同时派生自描线图元（零面积描线）与椭圆几何（角度语义、采样）。
// 椭圆外接框即插入时给出的矩形；角度是几何真相，改角度按上次相对框重新采样。
class ArcItem : public LineItemAbstract, public EllipseShape {
    Q_OBJECT
  public:
    explicit ArcItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("arc"); }
    QString DisplayName() const override;
    QRectF FrameRect() const override;

    qreal StartAngle() const { return m_startAngle; }
    qreal SweepAngle() const { return m_sweepAngle; }
    void SetAngles(qreal startDeg, qreal sweepDeg);

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

  private:
    // 角度变化后按“上次椭圆框相对内容框的比例”重建，拉伸过再改角度不会跳。
    void Rebuild();

    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
    QRectF m_refFrame{0.0, 0.0, 1.0, 1.0};
};
