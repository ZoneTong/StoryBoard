#pragma once

#include "I18n.h"
#include "LineAbstractItem.h"

#include <QRectF>
#include <QSizeF>
#include <QVector>

// 正弦波形图元：描线族的参数化子型。周期数与振幅是几何真相，几何按参数重新采样；
// 振幅是相对中心线的像素高度（波峰到中心线的距离，2 倍振幅即波高）；
// 竖直拉伸按新高度反推振幅，横向拉伸只拉宽一个周期，二者都回写到参数。
class SineItem : public LineAbstractItem {
    Q_OBJECT
  public:
    struct Params {
        int cycles = 1;         // 周期数 1–12
        qreal amplitude = 20.0; // 振幅（px，相对中心线），默认 20 与旧 128×40 波形一致
    };

    explicit SineItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("sine"); }
    QString DisplayName() const override;
    QRectF FrameRect() const override;

    Params params() const { return m_params; }
    void SetParams(const Params& params);

    // 插入入口：在场景矩形内按当前参数生成波形。
    void SetWaveGeometry(const QPointF& topLeftScene, const QSizeF& size, const QColor& color, qreal width);

    bool SupportsSineWave() const override { return true; }
    ParamState paramState() const override;
    void ApplyParamState(const ParamState& state) override;
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;

    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;

  protected:
    // 竖直拉伸直接改振幅：拖高拖矮等价于改波高，参数与几何始终一致。
    void ContentResized() override;

  private:
    // 参数变化后按“上次波形框相对内容框的比例”重建，保证拉伸过再改参数不会跳。
    void Rebuild();
    QVector<QPointF> WavePoints(const QRectF& frameScene) const;

    Params m_params;
    QRectF m_refFrame{0.0, 0.0, 1.0, 1.0};
    bool m_rebuilding = false;
};
