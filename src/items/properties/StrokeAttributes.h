#pragma once

#include <QColor>
#include <QJsonObject>
#include <Qt>
#include <QtGlobal>

// 描线属性基类：颜色、线宽、线型三件套的唯一定义处。
// 有面积图元同时多继承 FillAttributes 与 StrokeAttributes，
// 直线 / 曲线 / 弧线这类零面积图元只继承本类。
struct StrokeAttributesState {
    QColor color = QColor(30, 41, 59);
    qreal width = 4.0;
    Qt::PenStyle style = Qt::SolidLine;

    bool operator==(const StrokeAttributesState& other) const
    {
        return color == other.color && qFuzzyCompare(width + 1.0, other.width + 1.0) && style == other.style;
    }
    bool operator!=(const StrokeAttributesState& other) const { return !(*this == other); }
};

class StrokeAttributes {
  public:
    StrokeAttributes() = default;
    virtual ~StrokeAttributes() = default;

    virtual QColor StrokeColor() const { return m_strokeColor; }
    virtual qreal StrokeWidth() const { return m_strokeWidth; }
    virtual Qt::PenStyle StrokeStyle() const { return m_strokeStyle; }
    // 直线 / 曲线沿用的旧名字，语义就是线型。
    Qt::PenStyle strokePattern() const { return StrokeStyle(); }

    virtual void SetStrokeColor(const QColor& color);
    virtual void SetStrokeWidth(qreal width);
    virtual void SetStrokeStyle(Qt::PenStyle style);
    virtual void SetStrokePattern(Qt::PenStyle style) { SetStrokeStyle(style); }
    virtual void SetStroke(const QColor& color, qreal width);

    StrokeAttributesState StrokeState() const;
    void ApplyStrokeState(const StrokeAttributesState& state);

    void WriteStrokeJson(QJsonObject& json) const;
    void ReadStrokeJson(const QJsonObject& json);

  protected:
    virtual void StrokeChanged() {}

    QColor m_strokeColor = QColor(30, 41, 59);
    qreal m_strokeWidth = 4.0;
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
};
