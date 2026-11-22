#include "items/properties/StrokeAttributes.h"

#include <QtGlobal>

StrokeAttributesState StrokeAttributes::StrokeState() const
{
    StrokeAttributesState state;
    state.color = StrokeColor();
    state.width = StrokeWidth();
    state.style = StrokeStyle();
    return state;
}

void StrokeAttributes::ApplyStrokeState(const StrokeAttributesState& state)
{
    SetStroke(state.color, state.width);
    SetStrokeStyle(state.style);
    StrokeChanged();
}

void StrokeAttributes::SetStrokeColor(const QColor& color)
{
    const QColor resolved = color.isValid() ? color : QColor(30, 41, 59);
    if (m_strokeColor == resolved) {
        return;
    }
    m_strokeColor = resolved;
    StrokeChanged();
}

void StrokeAttributes::SetStrokeWidth(qreal width)
{
    const qreal resolved = qBound<qreal>(0.5, width, 48.0);
    if (qFuzzyCompare(m_strokeWidth + 1.0, resolved + 1.0)) {
        return;
    }
    m_strokeWidth = resolved;
    StrokeChanged();
}

void StrokeAttributes::SetStrokeStyle(Qt::PenStyle style)
{
    if (m_strokeStyle == style) {
        return;
    }
    m_strokeStyle = style;
    StrokeChanged();
}

void StrokeAttributes::SetStroke(const QColor& color, qreal width)
{
    SetStrokeColor(color);
    SetStrokeWidth(width);
    StrokeChanged();
}

void StrokeAttributes::WriteStrokeJson(QJsonObject& json) const
{
    json.insert(QStringLiteral("strokeColor"), StrokeColor().name(QColor::HexArgb));
    json.insert(QStringLiteral("strokeWidth"), StrokeWidth());
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(StrokeStyle()));
}

void StrokeAttributes::ReadStrokeJson(const QJsonObject& json)
{
    m_strokeColor = QColor(json.value(QStringLiteral("strokeColor")).toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid()) {
        m_strokeColor = QColor(30, 41, 59);
    }
    m_strokeWidth = qBound<qreal>(0.5, json.value(QStringLiteral("strokeWidth")).toDouble(4.0), 48.0);
    m_strokeStyle = static_cast<Qt::PenStyle>(qBound(0, json.value(QStringLiteral("strokeStyle")).toInt(1), 5));
}
