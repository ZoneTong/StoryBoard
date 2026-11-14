#include "ShapePathBuilder.h"

#include <QTransform>
#include <QtMath>

namespace {

QRectF normalizedRect(const QRectF& rect)
{
    return rect.normalized();
}

QPainterPath bracePath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0) {
        return QPainterPath();
    }

    const qreal xOuter = r.left() + w * 0.82;
    const qreal xMid = r.left() + w * 0.60;
    const qreal xTip = r.left() + w * 0.16;

    QPainterPath path;
    path.moveTo(xOuter, r.top());
    path.cubicTo(xMid, r.top() + h * 0.025, xMid, r.top() + h * 0.14, xMid, r.top() + h * 0.30);
    path.cubicTo(xMid, r.top() + h * 0.43, xTip, r.top() + h * 0.42, xTip, r.center().y());
    path.cubicTo(xTip, r.top() + h * 0.58, xMid, r.top() + h * 0.57, xMid, r.top() + h * 0.70);
    path.cubicTo(xMid, r.top() + h * 0.86, xMid, r.bottom() - h * 0.025, xOuter, r.bottom());
    return path;
}

QPainterPath starPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return QPainterPath();
    }

    QPainterPath path;
    constexpr int points = 10;
    for (int i = 0; i < points; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
        const qreal factor = (i % 2 == 0) ? 1.0 : 0.42;
        const QPointF point(center.x() + qCos(angle) * rx * factor, center.y() + qSin(angle) * ry * factor);
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

QPainterPath calloutPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0) {
        return QPainterPath();
    }

    const QRectF body(r.left(), r.top(), w, h * 0.78);
    const qreal radius = qMin(w, h) * 0.16;
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(body, radius, radius);

    QPainterPath tail;
    tail.moveTo(r.left() + w * 0.20, body.bottom() - 0.5);
    tail.lineTo(r.left() + w * 0.12, r.bottom());
    tail.lineTo(r.left() + w * 0.38, body.bottom() - 0.5);
    tail.closeSubpath();

    return bodyPath.united(tail);
}

} // namespace

QPainterPath ShapePathBuilder::mapUnitPath(const QPainterPath& unitPath, const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }
    QTransform transform;
    transform.translate(r.left(), r.top());
    transform.scale(r.width(), r.height());
    return transform.map(unitPath);
}

QPainterPath ShapePathBuilder::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return pathFor(type, rect, cornerRadius, 30.0, 120.0, -1.0);
}

QPainterPath ShapePathBuilder::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                       qreal sweepAngle, qreal shapeRadius)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0) {
        return QPainterPath();
    }

    // 扇形 / 弧线由椭圆派生：半径缺省取内容矩形宽度的一半（即整椭圆），
    // 显式半径按内容矩形的宽高比同步缩放，拉伸图元时仍保持椭圆形状。
    auto arcRect = [&r, shapeRadius]() {
        const qreal maximum = r.width() / 2.0;
        const qreal radius = shapeRadius >= 0.0 ? qBound<qreal>(0.0, shapeRadius, maximum) : maximum;
        const qreal ry = radius * r.height() / r.width();
        return QRectF(r.center().x() - radius, r.center().y() - ry, radius * 2.0, ry * 2.0);
    };
    auto arcSweep = [sweepAngle]() {
        const qreal sweep = qBound<qreal>(-360.0, sweepAngle, 360.0);
        // 扫过角为 0 时画不出可见图形，退回默认 120°，避免插入后“什么都没有”。
        return qFuzzyIsNull(sweep) ? 120.0 : sweep;
    };

    QPainterPath path;
    switch (type) {
    case ShapeType::Rectangle:
        if (cornerRadius > 0.0) {
            const qreal radius = qMin(cornerRadius, qMin(r.width(), r.height()) / 2.0);
            path.addRoundedRect(r, radius, radius);
        } else {
            path.addRect(r);
        }
        break;
    case ShapeType::RoundedRectangle: {
        const qreal radius = cornerRadius >= 0.0 ? qMin(cornerRadius, qMin(r.width(), r.height()) / 2.0)
                                                 : qMin(r.width(), r.height()) * 0.16;
        path.addRoundedRect(r, radius, radius);
        break;
    }
    case ShapeType::Ellipse:
        path.addEllipse(r);
        break;
    case ShapeType::Triangle:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
        break;
    case ShapeType::Diamond:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.center().x(), r.bottom());
        path.lineTo(r.left(), r.center().y());
        path.closeSubpath();
        break;
    case ShapeType::Star:
        path = starPath(r);
        break;
    case ShapeType::Arrow: {
        const qreal headLength = qMin(r.width() * 0.45, qMax(r.height() * 0.75, r.width() * 0.22));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal headBase = r.right() - headLength;
        path.moveTo(r.left(), cy - shaftHalf);
        path.lineTo(headBase, cy - shaftHalf);
        path.lineTo(headBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(headBase, r.bottom());
        path.lineTo(headBase, cy + shaftHalf);
        path.lineTo(r.left(), cy + shaftHalf);
        path.closeSubpath();
        break;
    }
    case ShapeType::DoubleArrow: {
        const qreal headLength =
            qMin(r.width() * 0.45, qMin(r.width() * 0.30, qMax(r.height() * 0.70, r.width() * 0.16)));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal leftBase = r.left() + headLength;
        const qreal rightBase = r.right() - headLength;
        path.moveTo(r.left(), cy);
        path.lineTo(leftBase, r.top());
        path.lineTo(leftBase, cy - shaftHalf);
        path.lineTo(rightBase, cy - shaftHalf);
        path.lineTo(rightBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(rightBase, r.bottom());
        path.lineTo(rightBase, cy + shaftHalf);
        path.lineTo(leftBase, cy + shaftHalf);
        path.lineTo(leftBase, r.bottom());
        path.closeSubpath();
        break;
    }
    case ShapeType::Line:
        path.moveTo(r.left(), r.center().y());
        path.lineTo(r.right(), r.center().y());
        break;
    case ShapeType::BraceLeft:
        path = bracePath(r);
        break;
    case ShapeType::BraceRight: {
        const QPainterPath left = bracePath(r);
        QTransform mirror;
        mirror.translate(r.left() + r.right(), 0.0);
        mirror.scale(-1.0, 1.0);
        path = mirror.map(left);
        break;
    }
    case ShapeType::Callout:
        path = calloutPath(r);
        break;
    case ShapeType::Sector:
        path.arcMoveTo(arcRect(), startAngle);
        path.arcTo(arcRect(), startAngle, arcSweep());
        path.lineTo(r.center());
        path.closeSubpath();
        break;
    case ShapeType::Arc:
        path.arcMoveTo(arcRect(), startAngle);
        path.arcTo(arcRect(), startAngle, arcSweep());
        break;
    }
    return path;
}
