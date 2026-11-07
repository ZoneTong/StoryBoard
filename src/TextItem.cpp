#include "TextItem.h"

#include <QFontMetricsF>
#include <QJsonObject>
#include <QPainter>
#include <QtMath>

namespace {
constexpr qreal kPadding = 12.0;
constexpr qreal kDefaultPointSize = 32.0;

QFont normalizedFont(QFont font)
{
    if (font.pointSizeF() <= 0.0)
        font.setPointSizeF(kDefaultPointSize);
    return font;
}

} // namespace

TextItem::TextItem(QGraphicsItem* parent)
    : BoardItem(parent)
{
    m_font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    m_font.setPointSizeF(kDefaultPointSize);
    m_baseSize = measure(QString(), m_font);
}

QString TextItem::displayName() const
{
    QString snippet = m_text.simplified();
    if (snippet.size() > 18)
        snippet = snippet.left(18) + QStringLiteral("…");
    return snippet.isEmpty() ? QStringLiteral("文字")
                             : QStringLiteral("文字：%1").arg(snippet);
}

QSizeF TextItem::measure(const QString& text, const QFont& font)
{
    const QFont effective = normalizedFont(font);
    const QFontMetricsF metrics(effective);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    qreal width = 0.0;
    for (const QString& line : lines)
        width = qMax(width, metrics.horizontalAdvance(line));
    qreal height = metrics.lineSpacing() * qMax(1, lines.size());
    if (height <= 0.0)
        height = metrics.height();

    return QSizeF(qMax<qreal>(64.0, width + 2.0 * kPadding),
                  qMax<qreal>(32.0, height + 2.0 * kPadding));
}

void TextItem::setTextFormat(const QString& text, const QFont& font, const QColor& color)
{
    m_text = text;
    m_font = normalizedFont(font);
    m_color = color.isValid() ? color : QColor(30, 41, 59);
    m_baseSize = measure(m_text, m_font);
    setContentSize(m_baseSize);
    update();
}

TextItemState TextItem::state() const
{
    TextItemState result;
    result.text = m_text;
    result.font = m_font;
    result.color = m_color;
    result.pos = pos();
    result.size = contentSize();
    result.baseSize = m_baseSize;
    return result;
}

void TextItem::applyState(const TextItemState& state)
{
    m_text = state.text;
    m_font = normalizedFont(state.font);
    m_color = state.color.isValid() ? state.color : QColor(30, 41, 59);
    m_baseSize = state.baseSize.isEmpty() ? measure(m_text, m_font) : state.baseSize;

    prepareGeometryChange();
    setPos(state.pos);
    setContentSize(state.size.isEmpty() ? m_baseSize : state.size);
    update();
}

void TextItem::paintContent(QPainter* painter)
{
    if (m_baseSize.isEmpty() || contentSize().isEmpty())
        return;

    const qreal scaleX = contentSize().width() / m_baseSize.width();
    const qreal scaleY = contentSize().height() / m_baseSize.height();
    const QFontMetricsF metrics(m_font);
    const QStringList lines = m_text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    painter->save();
    painter->setClipRect(contentRect());
    painter->scale(scaleX, scaleY);
    painter->translate(kPadding, kPadding);
    painter->setFont(m_font);
    painter->setPen(m_color);

    if (m_text.isEmpty()) {
        painter->setPen(QColor(148, 163, 184));
        painter->drawText(QPointF(0.0, metrics.ascent()), QStringLiteral("空文字"));
    } else {
        qreal baseline = metrics.ascent();
        for (const QString& line : lines) {
            painter->drawText(QPointF(0.0, baseline), line);
            baseline += metrics.lineSpacing();
        }
    }
    painter->restore();
}

QJsonObject TextItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("text"), m_text);
    json.insert(QStringLiteral("color"), m_color.name(QColor::HexArgb));
    json.insert(QStringLiteral("fontFamily"), m_font.family());
    json.insert(QStringLiteral("fontSize"), m_font.pointSizeF());
    json.insert(QStringLiteral("fontWeight"), static_cast<int>(m_font.weight()));
    json.insert(QStringLiteral("italic"), m_font.italic());
    json.insert(QStringLiteral("baseWidth"), m_baseSize.width());
    json.insert(QStringLiteral("baseHeight"), m_baseSize.height());
    return json;
}

void TextItem::fromJson(const QJsonObject& json)
{
    m_text = json.value(QStringLiteral("text")).toString();

    m_font.setFamily(json.value(QStringLiteral("fontFamily"))
                         .toString(QStringLiteral("Microsoft YaHei UI")));
    const qreal pointSize = json.value(QStringLiteral("fontSize")).toDouble(kDefaultPointSize);
    m_font.setPointSizeF(qBound<qreal>(6.0, pointSize, 400.0));
    const int weight = json.value(QStringLiteral("fontWeight"))
                           .toInt(static_cast<int>(QFont::Normal));
    m_font.setWeight(static_cast<QFont::Weight>(qBound(1, weight, 1000)));
    m_font.setItalic(json.value(QStringLiteral("italic")).toBool(false));

    m_color = QColor(json.value(QStringLiteral("color"))
                         .toString(QStringLiteral("#ff1e293b")));
    if (!m_color.isValid())
        m_color = QColor(30, 41, 59);

    const QSizeF storedBase(json.value(QStringLiteral("baseWidth")).toDouble(0.0),
                            json.value(QStringLiteral("baseHeight")).toDouble(0.0));
    m_baseSize = (storedBase.width() > 0.0 && storedBase.height() > 0.0)
        ? storedBase
        : measure(m_text, m_font);

    applyBaseJson(json);
    if (contentSize().isEmpty())
        setContentSize(m_baseSize);
}
