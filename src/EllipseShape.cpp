#include "EllipseShape.h"

#include <QtMath>

EllipseShape::EllipseShape(const QRectF& frame) : m_frame(frame.normalized()) {}

qreal EllipseShape::clampedSweep(qreal sweepDeg)
{
    const qreal sweep = qBound<qreal>(-360.0, sweepDeg, 360.0);
    // 扫过角为 0 时画不出可见图形，退回默认 120°，避免插入后“什么都没有”。
    return qFuzzyIsNull(sweep) ? 120.0 : sweep;
}

QPainterPath EllipseShape::ellipse() const
{
    QPainterPath path;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0) {
        return path;
    }
    path.addEllipse(m_frame);
    return path;
}

QPainterPath EllipseShape::arc(qreal startDeg, qreal sweepDeg) const
{
    QPainterPath path;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0) {
        return path;
    }
    path.arcMoveTo(m_frame, startDeg);
    path.arcTo(m_frame, startDeg, clampedSweep(sweepDeg));
    return path;
}

QPainterPath EllipseShape::sector(qreal startDeg, qreal sweepDeg) const
{
    QPainterPath path;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0) {
        return path;
    }
    path.arcMoveTo(m_frame, startDeg);
    path.arcTo(m_frame, startDeg, clampedSweep(sweepDeg));
    path.lineTo(center());
    path.closeSubpath();
    return path;
}

QRectF EllipseShape::arcBounds(qreal startDeg, qreal sweepDeg) const
{
    return arc(startDeg, sweepDeg).boundingRect();
}

QRectF EllipseShape::sectorBounds(qreal startDeg, qreal sweepDeg) const
{
    return sector(startDeg, sweepDeg).boundingRect();
}

QVector<QPointF> EllipseShape::sample(qreal startDeg, qreal sweepDeg, int count) const
{
    QVector<QPointF> points;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0 || count < 1) {
        return points;
    }

    const qreal sweep = clampedSweep(sweepDeg);
    const qreal cx = center().x();
    const qreal cy = center().y();
    const qreal rx = radiusX();
    const qreal ry = radiusY();

    points.reserve(count + 1);
    for (int i = 0; i <= count; ++i) {
        const qreal t = qreal(i) / qreal(count);
        // Qt 角度：逆时针为正，故 y 取负号。
        const qreal rad = qDegreesToRadians(startDeg + sweep * t);
        points.append(QPointF(cx + rx * qCos(rad), cy - ry * qSin(rad)));
    }
    return points;
}
