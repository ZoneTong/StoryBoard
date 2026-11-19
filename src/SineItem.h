#pragma once

#include "CurveItem.h"

#include <QRectF>
#include <QSizeF>
#include <QVector>

// 正弦波形图元：曲线族的参数化子型。周期数与振幅是几何真相，几何按参数重新采样；
// 拉伸只等比缩放既有段，不改参数。
class SineItem : public CurveItem {
    Q_OBJECT
  public:
    struct Params {
        int cycles = 1;       // 周期数 1–12
        qreal amplitude = 0.5; // 振幅 / 内容高度，0.5 = 撑满内容框（与旧形态一致）
    };

    explicit SineItem(QGraphicsItem* parent = nullptr);

    QString displayName() const override;
    QRectF frameRect() const override;

    Params params() const { return m_params; }
    void setParams(const Params& params);

    // 插入入口：在场景矩形内按当前参数生成波形。
    void setWaveGeometry(const QPointF& topLeftScene, const QSizeF& size, const QColor& color, qreal width);

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

  private:
    // 参数变化后按“上次波形框相对内容框的比例”重建，保证拉伸过再改参数不会跳。
    void rebuild();
    QVector<QPointF> wavePoints(const QRectF& frameScene) const;

    Params m_params;
    QRectF m_relFrame{0.0, 0.0, 1.0, 1.0};
};
