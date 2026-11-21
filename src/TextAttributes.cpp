#include "TextAttributes.h"

#include <QFontMetricsF>
#include <QJsonValue>
#include <QStringList>

namespace {

constexpr qreal PADDING = 12.0;
constexpr qreal DEFAULT_POINT_SIZE = 32.0;
constexpr qreal MINIMUM_WIDTH = 64.0;
constexpr qreal MINIMUM_HEIGHT = 32.0;

QFont NormalizedFont(QFont font)
{
    if (font.pointSizeF() <= 0.0) {
        font.setPointSizeF(DEFAULT_POINT_SIZE);
    }
    return font;
}

QColor NormalizedColor(const QColor& color)
{
    return color.isValid() ? color : QColor(30, 41, 59);
}

} // namespace

TextAttributes::TextAttributes()
{
    m_font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    m_font.setPointSizeF(DEFAULT_POINT_SIZE);
    m_baseSize = Measure(QString(), m_font);
}

void TextAttributes::SetTextContent(const QString& text)
{
    SetTextFormat(text, m_font, m_color);
}

void TextAttributes::SetTextFont(const QFont& font)
{
    SetTextFormat(m_text, font, m_color);
}

void TextAttributes::SetTextColor(const QColor& color)
{
    SetTextFormat(m_text, m_font, color);
}

void TextAttributes::SetTextFormat(const QString& text, const QFont& font, const QColor& color)
{
    m_text = text;
    m_font = NormalizedFont(font);
    m_color = NormalizedColor(color);
    m_baseSize = Measure(m_text, m_font);
    TextChanged();
}

TextAttributesState TextAttributes::TextAttributeState() const
{
    return TextAttributesState{m_text, m_font, m_color, m_baseSize};
}

void TextAttributes::ApplyTextAttributeState(const TextAttributesState& state)
{
    m_text = state.text;
    m_font = NormalizedFont(state.font);
    m_color = NormalizedColor(state.color);
    m_baseSize = state.baseSize.isEmpty() ? Measure(m_text, m_font) : state.baseSize;
    TextChanged();
}

void TextAttributes::WriteTextJson(QJsonObject& json) const
{
    json.insert(QStringLiteral("text"), m_text);
    json.insert(QStringLiteral("color"), m_color.name(QColor::HexArgb));
    json.insert(QStringLiteral("fontFamily"), m_font.family());
    json.insert(QStringLiteral("fontSize"), m_font.pointSizeF());
    json.insert(QStringLiteral("fontWeight"), static_cast<int>(m_font.weight()));
    json.insert(QStringLiteral("italic"), m_font.italic());
    json.insert(QStringLiteral("underline"), m_font.underline());
    json.insert(QStringLiteral("strikeOut"), m_font.strikeOut());
    json.insert(QStringLiteral("baseWidth"), m_baseSize.width());
    json.insert(QStringLiteral("baseHeight"), m_baseSize.height());
}

void TextAttributes::ReadTextJson(const QJsonObject& json)
{
    m_text = json.value(QStringLiteral("text")).toString();

    m_font.setFamily(json.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Microsoft YaHei UI")));
    const qreal pointSize = json.value(QStringLiteral("fontSize")).toDouble(DEFAULT_POINT_SIZE);
    m_font.setPointSizeF(qBound<qreal>(6.0, pointSize, 400.0));
    const int weight = json.value(QStringLiteral("fontWeight")).toInt(static_cast<int>(QFont::Normal));
    m_font.setWeight(static_cast<QFont::Weight>(qBound(1, weight, 1000)));
    m_font.setItalic(json.value(QStringLiteral("italic")).toBool(false));
    m_font.setUnderline(json.value(QStringLiteral("underline")).toBool(false));
    m_font.setStrikeOut(json.value(QStringLiteral("strikeOut")).toBool(false));

    m_color = QColor(json.value(QStringLiteral("color")).toString(QStringLiteral("#ff1e293b")));
    if (!m_color.isValid()) {
        m_color = QColor(30, 41, 59);
    }

    const QSizeF storedBase(json.value(QStringLiteral("baseWidth")).toDouble(0.0),
                            json.value(QStringLiteral("baseHeight")).toDouble(0.0));
    m_baseSize = (storedBase.width() > 0.0 && storedBase.height() > 0.0) ? storedBase : Measure(m_text, m_font);
    TextChanged();
}

QSizeF TextAttributes::Measure(const QString& text, const QFont& font)
{
    const QFont effective = NormalizedFont(font);
    const QFontMetricsF metrics(effective);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    qreal width = 0.0;
    for (const QString& line : lines) {
        width = qMax(width, metrics.horizontalAdvance(line));
    }
    qreal height = metrics.lineSpacing() * qMax(1, lines.size());
    if (height <= 0.0) {
        height = metrics.height();
    }

    return QSizeF(qMax<qreal>(MINIMUM_WIDTH, width + 2.0 * PADDING),
                  qMax<qreal>(MINIMUM_HEIGHT, height + 2.0 * PADDING));
}
