#include "ArrowStyle.h"

#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QtMath>
#include <cmath>

namespace {

qreal headLengthFor(qreal strokeWidth)
{
    // 头长固定为 4 倍线宽；线宽为 0 时退化为 4px，保证箭头仍可见。
    return qMax<qreal>(4.0, strokeWidth * 4.0);
}

QPointF normalizedDir(const QPointF& tip, const QPointF& toward)
{
    const QPointF delta = tip - toward;
    const qreal length = std::hypot(delta.x(), delta.y());
    if (length <= 0.0) {
        return QPointF(1.0, 0.0);
    }
    return QPointF(delta.x() / length, delta.y() / length);
}

} // namespace

QList<ArrowStyle> allArrowStyles()
{
    return {ArrowStyle::None, ArrowStyle::TriangleFilled, ArrowStyle::TriangleHollow,
            ArrowStyle::OpenArrow, ArrowStyle::Dot, ArrowStyle::Bar};
}

QString arrowStyleName(ArrowStyle style)
{
    switch (style) {
    case ArrowStyle::None:
        return QStringLiteral("无");
    case ArrowStyle::TriangleFilled:
        return QStringLiteral("实心三角");
    case ArrowStyle::TriangleHollow:
        return QStringLiteral("空心三角");
    case ArrowStyle::OpenArrow:
        return QStringLiteral("开口箭头");
    case ArrowStyle::Dot:
        return QStringLiteral("圆点");
    case ArrowStyle::Bar:
        return QStringLiteral("竖杠");
    }
    return QStringLiteral("无");
}

QString arrowStyleToString(ArrowStyle style)
{
    switch (style) {
    case ArrowStyle::TriangleFilled:
        return QStringLiteral("triangle");
    case ArrowStyle::TriangleHollow:
        return QStringLiteral("hollow-triangle");
    case ArrowStyle::OpenArrow:
        return QStringLiteral("open");
    case ArrowStyle::Dot:
        return QStringLiteral("dot");
    case ArrowStyle::Bar:
        return QStringLiteral("bar");
    case ArrowStyle::None:
        break;
    }
    return QStringLiteral("none");
}

ArrowStyle arrowStyleFromString(const QString& value)
{
    if (value == QStringLiteral("triangle")) {
        return ArrowStyle::TriangleFilled;
    }
    if (value == QStringLiteral("hollow-triangle")) {
        return ArrowStyle::TriangleHollow;
    }
    if (value == QStringLiteral("open")) {
        return ArrowStyle::OpenArrow;
    }
    if (value == QStringLiteral("dot")) {
        return ArrowStyle::Dot;
    }
    if (value == QStringLiteral("bar")) {
        return ArrowStyle::Bar;
    }
    // 旧文件没有箭头字段时按「无」处理，与历史外观一致。
    return ArrowStyle::None;
}

bool arrowStyleIsHead(ArrowStyle style)
{
    return style != ArrowStyle::None;
}

QPainterPath arrowHeadPath(const QPointF& tip, const QPointF& toward, qreal strokeWidth, ArrowStyle style)
{
    QPainterPath path;
    if (!arrowStyleIsHead(style)) {
        return path;
    }
    const QPointF dir = normalizedDir(tip, toward);
    const QPointF perp(-dir.y(), dir.x());
    const qreal length = headLengthFor(strokeWidth);
    const QPointF base = tip - dir * length;

    switch (style) {
    case ArrowStyle::TriangleFilled:
    case ArrowStyle::TriangleHollow: {
        // 张角 90°：两翼从中轴偏 ±45°，横向偏移等于头长。
        const QPointF wingA = base + perp * length;
        const QPointF wingB = base - perp * length;
        path.moveTo(tip);
        path.lineTo(wingA);
        path.lineTo(wingB);
        path.closeSubpath();
        break;
    }
    case ArrowStyle::OpenArrow: {
        const QPointF wingA = base + perp * length;
        const QPointF wingB = base - perp * length;
        path.moveTo(wingA);
        path.lineTo(tip);
        path.lineTo(wingB);
        break;
    }
    case ArrowStyle::Dot: {
        const qreal radius = qMax<qreal>(1.5, strokeWidth * 1.5);
        path.addEllipse(tip, radius, radius);
        break;
    }
    case ArrowStyle::Bar: {
        const qreal half = qMax<qreal>(2.0, length / 2.0);
        path.moveTo(tip + perp * half);
        path.lineTo(tip - perp * half);
        break;
    }
    case ArrowStyle::None:
        break;
    }
    return path;
}

void paintArrowHead(QPainter* painter, const QPointF& tip, const QPointF& toward, qreal strokeWidth,
                    const QColor& color, ArrowStyle style)
{
    if (!painter || !arrowStyleIsHead(style)) {
        return;
    }
    const QPainterPath path = arrowHeadPath(tip, toward, strokeWidth, style);
    if (path.isEmpty()) {
        return;
    }
    QPen pen(color, qMax<qreal>(1.0, strokeWidth), Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    painter->setPen(pen);
    const bool filled = style == ArrowStyle::TriangleFilled || style == ArrowStyle::Dot;
    painter->setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
    painter->drawPath(path);
}
