#include "TextItem.h"

#include "CanvasItemVisitor.h"
#include "I18n.h"

#include <QFontMetricsF>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {
constexpr qreal TEXT_PADDING = 12.0;
}

TextItem::TextItem(QGraphicsItem* parent) : CanvasItem(parent) {}

void TextItem::Accept(CanvasItemVisitor& visitor)
{
    visitor.VisitText(*this);
}

ItemPropertySet TextItem::Properties() const
{
    return CanvasItem::Properties().United(
        ItemPropertySet{ItemProperty::TEXT_CONTENT, ItemProperty::TEXT_COLOR, ItemProperty::TEXT_FONT});
}

QVariant TextItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::TEXT_CONTENT:
        return TextContent();
    case ItemProperty::TEXT_COLOR:
        return TextColor();
    case ItemProperty::TEXT_FONT:
        return TextFont();
    default:
        return CanvasItem::PropertyValue(property);
    }
}

void TextItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::TEXT_CONTENT:
        SetTextFormat(value.toString(), TextFont(), TextColor());
        break;
    case ItemProperty::TEXT_COLOR:
        SetTextFormat(TextContent(), TextFont(), value.value<QColor>());
        break;
    case ItemProperty::TEXT_FONT:
        SetTextFormat(TextContent(), value.value<QFont>(), TextColor());
        break;
    default:
        CanvasItem::SetPropertyValue(property, value);
        break;
    }
}

QString TextItem::DisplayName() const
{
    QString snippet = TextContent().simplified();
    if (snippet.size() > 18) {
        snippet = snippet.left(18) + QStringLiteral("…");
    }
    return snippet.isEmpty() ? tr("Text") : tr("Text: %1").arg(snippet);
}

QString TextItem::TypeDisplayName() const
{
    return tr("Text");
}

QSizeF TextItem::Measure(const QString& text, const QFont& font)
{
    return TextAttributes::Measure(text, font);
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
    const TextAttributesState text = TextAttributeState();
    result.text = text.text;
    result.font = text.font;
    result.color = text.color;
    result.baseSize = text.baseSize;
    result.pos = pos();
    result.size = ContentSize();
    return result;
}

void TextItem::ApplyState(const TextItemState& state)
{
    ApplyTextAttributeState(TextAttributesState{state.text, state.font, state.color, state.baseSize});
    setPos(state.pos);
    SetContentSize(state.size.isEmpty() ? TextNaturalSize() : state.size);
    update();
}

void TextItem::TextChanged()
{
    SetContentSize(TextNaturalSize());
    update();
}

void TextItem::PaintContent(QPainter* painter)
{
    const QSizeF baseSize = TextNaturalSize();
    const QFont font = TextFont();
    const QString text = TextContent();
    if (baseSize.isEmpty() || ContentSize().isEmpty()) {
        return;
    }

    const qreal scaleX = ContentSize().width() / baseSize.width();
    const qreal scaleY = ContentSize().height() / baseSize.height();
    const QFontMetricsF metrics(font);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    painter->save();
    // IntersectClip：保留 DrawItems() 设置的“被遮挡裁剪”区域，不能整个替换。
    painter->setClipRect(ContentRect(), Qt::IntersectClip);
    painter->scale(scaleX, scaleY);
    painter->translate(TEXT_PADDING, TEXT_PADDING);
    painter->setFont(font);
    painter->setPen(TextColor());

    if (text.isEmpty()) {
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
    const QString text = TextContent();
    const QFont font = TextFont();
    if (ContentSize().isEmpty() || TextColor().alpha() == 0) {
        return QPainterPath();
    }

    const QSizeF base = TextNaturalSize().isEmpty() ? QSizeF(1.0, 1.0) : TextNaturalSize();
    const qreal scaleX = ContentSize().width() / base.width();
    const qreal scaleY = ContentSize().height() / base.height();
    const QFontMetricsF metrics(font);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    // 文字没有底色，只有字形占住画布：用字形轮廓（而不是整块行高矩形）作为
    // “涂满区域”，这样文字既能遮挡下层，字缝之间又不会挡住下层颜色。
    QPainterPath path;
    qreal baseline = metrics.ascent();
    for (const QString& line : lines) {
        if (!line.isEmpty()) {
            QPainterPath linePath;
            linePath.addText(TEXT_PADDING, baseline, font, line);
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
    WriteTextJson(json);
    return json;
}

void TextItem::FromJson(const QJsonObject& json)
{
    ReadTextJson(json);
    ApplyBaseJson(json);
    if (ContentSize().isEmpty()) {
        SetContentSize(TextNaturalSize());
    }
}
