#pragma once

#include "CurveItem.h"
#include "EllipseShape.h"

// 弧线图元：同时派生自曲线（零面积描线）与椭圆几何（角度语义、采样）。
// 椭圆外接框即插入时给出的矩形；角度是几何真相，改角度按上次相对框重新采样。
class ArcItem : public CurveItem, public EllipseShape {
    Q_OBJECT
  public:
    explicit ArcItem(QGraphicsItem* parent = nullptr);

    QString displayName() const override;
    QRectF frameRect() const override;

    qreal startAngle() const { return m_startAngle; }
    qreal sweepAngle() const { return m_sweepAngle; }
    void setAngles(qreal startDeg, qreal sweepDeg);

    // 插入入口：椭圆框 + 起始角 / 扫过角。
    void setArcGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                        const QColor& color, qreal width);

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

  private:
    // 角度变化后按“上次椭圆框相对内容框的比例”重建，拉伸过再改角度不会跳。
    void rebuild();

    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
    QRectF m_relFrame{0.0, 0.0, 1.0, 1.0};
};
