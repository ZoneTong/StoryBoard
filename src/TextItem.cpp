#include "TextItem.h"

#include "I18n.h"

#include <QFontMetricsF>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr qreal PADDING = 12.0;
constexpr qreal DEFAULT_POINT_SIZE = 32.0;

QFont normalizedFont(QFont font)
{
    if (font.pointSizeF() <= 0.0) {
        font.setPointSizeF(DEFAULT_POINT_SIZE);
    }
    return font;
}

} // namespace

TextItem::TextItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    m_font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    m_font.setPointSizeF(DEFAULT_POINT_SIZE);
    m_baseSize = measure(QString(), m_font);
}

ItemPropertySet TextItem::Properties() const
{
    return CanvasItem::Properties().united(
        ItemPropertySet{ItemProperty::TEXT_CONTENT, ItemProperty::TEXT_COLOR, ItemProperty::TEXT_FONT});
}

QVariant TextItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::TEXT_CONTENT:
        return m_text;
    case ItemProperty::TEXT_COLOR:
        return m_color;
    case ItemProperty::TEXT_FONT:
        return m_font;
    default:
        return CanvasItem::PropertyValue(property);
    }
}

void TextItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::TEXT_CONTENT:
        setTextFormat(value.toString(), m_font, m_color);
        break;
    case ItemProperty::TEXT_COLOR:
        setTextFormat(m_text, m_font, value.value<QColor>());
        break;
    case ItemProperty::TEXT_FONT:
        setTextFormat(m_text, value.value<QFont>(), m_color);
        break;
    default:
        CanvasItem::SetPropertyValue(property, value);
        break;
    }
}

QString TextItem::DisplayName() const
{
    QString snippet = m_text.simplified();
    if (snippet.size() > 18) {
        snippet = snippet.left(18) + QStringLiteral("…");
    }
    return snippet.isEmpty() ? tr("Text") : tr("Text: %1").arg(snippet);
}

QString TextItem::TypeDisplayName() const
{
    return tr("Text");
}

QSizeF TextItem::measure(const QString& text, const QFont& font)
{
    const QFont effective = normalizedFont(font);
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

    return QSizeF(qMax<qreal>(64.0, width + 2.0 * PADDING), qMax<qreal>(32.0, height + 2.0 * PADDING));
}

void TextItem::setTextFormat(const QString& text, const QFont& font, const QColor& color)
{
    m_text = text;
    m_font = normalizedFont(font);
    m_color = color.isValid() ? color : QColor(30, 41, 59);
    m_baseSize = measure(m_text, m_font);
    SetContentSize(m_baseSize);
    update();
}

TextItemState TextItem::TextState() const
{
    return State();
}

void TextItem::ApplyTextState(const TextItemState& state)
{
    ApplyState(state);
}

TextItemState TextItem::State() const
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

void TextItem::ApplyState(const TextItemState& state)
{
    m_text = state.text;
    m_font = normalizedFont(state.font);
    m_color = state.color.isValid() ? state.color : QColor(30, 41, 59);
    m_baseSize = state.baseSize.isEmpty() ? measure(m_text, m_font) : state.baseSize;

    // 不在这里额外 prepareGeometryChange：setContentSize 会在尺寸真正变化时
    // 负责声明几何变化；重复调用会让旧的绘制区域没有被正确纳入重绘，留下旧
    // 字形的残影。
    setPos(state.pos);
    SetContentSize(state.size.isEmpty() ? m_baseSize : state.size);
    update();
}

void TextItem::PaintContent(QPainter* painter)
{
    if (m_baseSize.isEmpty() || contentSize().isEmpty()) {
        return;
    }

    const qreal scaleX = contentSize().width() / m_baseSize.width();
    const qreal scaleY = contentSize().height() / m_baseSize.height();
    const QFontMetricsF metrics(m_font);
    const QStringList lines = m_text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    painter->save();
    // IntersectClip：保留 drawItems() 设置的“被遮挡裁剪”区域，不能整个替换。
    painter->setClipRect(ContentRect(), Qt::IntersectClip);
    painter->scale(scaleX, scaleY);
    painter->translate(PADDING, PADDING);
    painter->setFont(m_font);
    painter->setPen(m_color);

    if (m_text.isEmpty()) {
        painter->setPen(QColor(148, 163, 184));
        painter->drawText(QPointF(0.0, metrics.ascent()), tr("Empty text"));
    } else {
        qreal baseline = metrics.ascent();
        for (const QString& line : lines) {
            painter->drawText(QPointF(0.0, baseline), line);
            baseline += metrics.lineSpacing();
        }
    }
    painter->restore();
}

QPainterPath TextItem::OutlineShape() const
{
    QPainterPath path;
    path.addRect(ContentRect());
    return path;
}

QPainterPath TextItem::PaintedShape() const
{
    if (contentSize().isEmpty() || m_color.alpha() == 0) {
        return QPainterPath();
    }

    const QSizeF base = m_baseSize.isEmpty() ? QSizeF(1.0, 1.0) : m_baseSize;
    const qreal scaleX = contentSize().width() / base.width();
    const qreal scaleY = contentSize().height() / base.height();
    const QFontMetricsF metrics(m_font);
    const QStringList lines = m_text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    // 文字没有底色，只有字形占住画布：用字形轮廓（而不是整块行高矩形）作为
    // “涂满区域”，这样文字既能遮挡下层，字缝之间又不会挡住下层颜色。
    QPainterPath path;
    qreal baseline = metrics.ascent();
    for (const QString& line : lines) {
        if (!line.isEmpty()) {
            QPainterPath linePath;
            linePath.addText(PADDING, baseline, m_font, line);
            path = path.united(linePath);
        }
        baseline += metrics.lineSpacing();
    }

    if (path.isEmpty()) {
        return CanvasItem::PaintedShape();
    }

    QTransform transform;
    transform.scale(scaleX, scaleY);
    QPainterPath scaled = transform.map(path);

    QPainterPath contentPath;
    contentPath.addRect(ContentRect());
    return scaled.intersected(contentPath);
}

QPainterPath TextItem::ClippingShape() const
{
    // 文字直接叠在下层上绘制，不参与下层的遮挡裁剪。按字形裁剪下层会
    // 在低缩放和亚像素位置露出画板底色，表现为黑字后面的白色错位重影。
    return QPainterPath();
}

QJsonObject TextItem::ToJson() const
{
    QJsonObject json = BaseJson();
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
    return json;
}

void TextItem::FromJson(const QJsonObject& json)
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
    m_baseSize = (storedBase.width() > 0.0 && storedBase.height() > 0.0) ? storedBase : measure(m_text, m_font);

    ApplyBaseJson(json);
    if (contentSize().isEmpty()) {
        SetContentSize(m_baseSize);
    }
}
