#include "items/properties/ArrowStyle.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QtMath>
#include <cmath>

namespace {

qreal HeadLengthFor(qreal strokeWidth)
{
    // 头长固定为 4 倍线宽；线宽为 0 时退化为 4px，保证箭头仍可见。
    return qMax<qreal>(4.0, strokeWidth * 4.0);
}

QPointF NormalizedDir(const QPointF& tip, const QPointF& toward)
{
    const QPointF delta = tip - toward;
    const qreal length = std::hypot(delta.x(), delta.y());
    if (length <= 0.0) {
        return QPointF(1.0, 0.0);
    }
    return QPointF(delta.x() / length, delta.y() / length);
}

} // namespace

QList<ArrowStyle> AllArrowStyles()
{
    return {ArrowStyle::NONE,
            ArrowStyle::TRIANGLE_FILLED,
            ArrowStyle::TRIANGLE_HOLLOW,
            ArrowStyle::OPEN_ARROW,
            ArrowStyle::DOT,
            ArrowStyle::BAR};
}

namespace ArrowStyles {

QString Name(ArrowStyle style)
{
    auto translate = [](const char* text) { return QCoreApplication::translate("ArrowStyle", text); };
    switch (style) {
    case ArrowStyle::NONE:
        return translate("None");
    case ArrowStyle::TRIANGLE_FILLED:
        return translate("Filled triangle");
    case ArrowStyle::TRIANGLE_HOLLOW:
        return translate("Hollow triangle");
    case ArrowStyle::OPEN_ARROW:
        return translate("Open arrow");
    case ArrowStyle::DOT:
        return translate("Dot");
    case ArrowStyle::BAR:
        return translate("Bar");
    }
    return translate("None");
}

QString LogName(ArrowStyle style)
{
    switch (style) {
    case ArrowStyle::NONE:
        return QStringLiteral("None");
    case ArrowStyle::TRIANGLE_FILLED:
        return QStringLiteral("Filled triangle");
    case ArrowStyle::TRIANGLE_HOLLOW:
        return QStringLiteral("Hollow triangle");
    case ArrowStyle::OPEN_ARROW:
        return QStringLiteral("Open arrow");
    case ArrowStyle::DOT:
        return QStringLiteral("Dot");
    case ArrowStyle::BAR:
        return QStringLiteral("Bar");
    }
    return QStringLiteral("None");
}

} // namespace ArrowStyles


QString ArrowStyleToString(ArrowStyle style)
{
    switch (style) {
    case ArrowStyle::TRIANGLE_FILLED:
        return QStringLiteral("triangle");
    case ArrowStyle::TRIANGLE_HOLLOW:
        return QStringLiteral("hollow-triangle");
    case ArrowStyle::OPEN_ARROW:
        return QStringLiteral("open");
    case ArrowStyle::DOT:
        return QStringLiteral("dot");
    case ArrowStyle::BAR:
        return QStringLiteral("bar");
    case ArrowStyle::NONE:
        break;
    }
    return QStringLiteral("none");
}

ArrowStyle ArrowStyleFromString(const QString& value)
{
    if (value == QStringLiteral("triangle")) {
        return ArrowStyle::TRIANGLE_FILLED;
    }
    if (value == QStringLiteral("hollow-triangle")) {
        return ArrowStyle::TRIANGLE_HOLLOW;
    }
    if (value == QStringLiteral("open")) {
        return ArrowStyle::OPEN_ARROW;
    }
    if (value == QStringLiteral("dot")) {
        return ArrowStyle::DOT;
    }
    if (value == QStringLiteral("bar")) {
        return ArrowStyle::BAR;
    }
    // 旧文件没有箭头字段时按「无」处理，与历史外观一致。
    return ArrowStyle::NONE;
}

bool ArrowStyleIsHead(ArrowStyle style)
{
    return style != ArrowStyle::NONE;
}

QPainterPath ArrowHeadPath(const QPointF& tip, const QPointF& toward, qreal strokeWidth, ArrowStyle style)
{
    QPainterPath path;
    if (!ArrowStyleIsHead(style)) {
        return path;
    }
    const QPointF dir = NormalizedDir(tip, toward);
    const QPointF perp(-dir.y(), dir.x());
    const qreal length = HeadLengthFor(strokeWidth);
    const QPointF base = tip - dir * length;

    switch (style) {
    case ArrowStyle::TRIANGLE_FILLED:
    case ArrowStyle::TRIANGLE_HOLLOW: {
        // 张角 90°：两翼从中轴偏 ±45°，横向偏移等于头长。
        const QPointF wingA = base + perp * length;
        const QPointF wingB = base - perp * length;
        path.moveTo(tip);
        path.lineTo(wingA);
        path.lineTo(wingB);
        path.closeSubpath();
        break;
    }
    case ArrowStyle::OPEN_ARROW: {
        const QPointF wingA = base + perp * length;
        const QPointF wingB = base - perp * length;
        path.moveTo(wingA);
        path.lineTo(tip);
        path.lineTo(wingB);
        break;
    }
    case ArrowStyle::DOT: {
        const qreal radius = qMax<qreal>(1.5, strokeWidth * 1.5);
        path.addEllipse(tip, radius, radius);
        break;
    }
    case ArrowStyle::BAR: {
        const qreal half = qMax<qreal>(2.0, length / 2.0);
        path.moveTo(tip + perp * half);
        path.lineTo(tip - perp * half);
        break;
    }
    case ArrowStyle::NONE:
        break;
    }
    return path;
}

void PaintArrowHead(QPainter* painter, const QPointF& tip, const QPointF& toward, qreal strokeWidth,
                    const QColor& color, ArrowStyle style)
{
    if (!painter || !ArrowStyleIsHead(style)) {
        return;
    }
    const QPainterPath path = ArrowHeadPath(tip, toward, strokeWidth, style);
    if (path.isEmpty()) {
        return;
    }
    QPen pen(color, qMax<qreal>(1.0, strokeWidth), Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    painter->setPen(pen);
    const bool filled = style == ArrowStyle::TRIANGLE_FILLED || style == ArrowStyle::DOT;
    painter->setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
    painter->drawPath(path);
}
