#pragma once

#include "I18n.h"
#include "LineItemAbstract.h"

#include <QRectF>
#include <QSizeF>
#include <QVector>

// 正弦波形图元：描线族的参数化子型。周期数与振幅是几何真相，几何按参数重新采样；
// 振幅是相对中心线的像素高度（波峰到中心线的距离，2 倍振幅即波高）；
// 竖直拉伸按新高度反推振幅，横向拉伸只拉宽一个周期，二者都回写到参数。
class SineItem : public LineItemAbstract {
    Q_OBJECT
  public:
    struct Params {
        int cycles = 1;         // 周期数 1–12
        qreal amplitude = 20.0; // 振幅（px，相对中心线），默认 20 与旧 128×40 波形一致
    };

    explicit SineItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("sine"); }
    QString displayName() const override;
    QRectF frameRect() const override;

    Params params() const { return m_params; }
    void setParams(const Params& params);

    // 插入入口：在场景矩形内按当前参数生成波形。
    void setWaveGeometry(const QPointF& topLeftScene, const QSizeF& size, const QColor& color, qreal width);

    bool supportsSineWave() const override { return true; }
    ParamState paramState() const override;
    void applyParamState(const ParamState& state) override;
    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

  protected:
    // 竖直拉伸直接改振幅：拖高拖矮等价于改波高，参数与几何始终一致。
    void contentResized() override;

  private:
    // 参数变化后按“上次波形框相对内容框的比例”重建，保证拉伸过再改参数不会跳。
    void rebuild();
    QVector<QPointF> wavePoints(const QRectF& frameScene) const;

    Params m_params;
    QRectF m_refFrame{0.0, 0.0, 1.0, 1.0};
    bool m_rebuilding = false;
};
