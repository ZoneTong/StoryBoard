#include "EllipseShape.h"

#include <QtMath>

EllipseShape::EllipseShape(const QRectF& frame) : m_frame(frame.normalized()) {}

qreal EllipseShape::ClampedSweep(qreal sweepDeg)
{
    const qreal sweep = qBound<qreal>(-360.0, sweepDeg, 360.0);
    // 扫过角为 0 时画不出可见图形，退回默认 120°，避免插入后“什么都没有”。
    return qFuzzyIsNull(sweep) ? 120.0 : sweep;
}

QPainterPath EllipseShape::Ellipse() const
{
    QPainterPath path;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0) {
        return path;
    }
    path.addEllipse(m_frame);
    return path;
}

QPainterPath EllipseShape::Arc(qreal startDeg, qreal sweepDeg) const
{
    QPainterPath path;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0) {
        return path;
    }
    path.arcMoveTo(m_frame, startDeg);
    path.arcTo(m_frame, startDeg, ClampedSweep(sweepDeg));
    return path;
}

QPainterPath EllipseShape::Sector(qreal startDeg, qreal sweepDeg) const
{
    QPainterPath path;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0) {
        return path;
    }
    path.arcMoveTo(m_frame, startDeg);
    path.arcTo(m_frame, startDeg, ClampedSweep(sweepDeg));
    path.lineTo(Center());
    path.closeSubpath();
    return path;
}

QRectF EllipseShape::ArcBounds(qreal startDeg, qreal sweepDeg) const
{
    return Arc(startDeg, sweepDeg).boundingRect();
}

QRectF EllipseShape::SectorBounds(qreal startDeg, qreal sweepDeg) const
{
    return Sector(startDeg, sweepDeg).boundingRect();
}

QPointF EllipseShape::PointOnFrame(const QRectF& frame, qreal deg)
{
    const QRectF normalized = frame.normalized();
    const qreal rad = qDegreesToRadians(deg);
    // Qt 角度：逆时针为正，故 y 取负号。
    return QPointF(normalized.center().x() + normalized.width() / 2.0 * qCos(rad),
                   normalized.center().y() - normalized.height() / 2.0 * qSin(rad));
}

QVector<QPointF> EllipseShape::Sample(qreal startDeg, qreal sweepDeg, int count) const
{
    QVector<QPointF> points;
    if (m_frame.width() <= 0.0 || m_frame.height() <= 0.0 || count < 1) {
        return points;
    }

    const qreal sweep = ClampedSweep(sweepDeg);
    const qreal cx = Center().x();
    const qreal cy = Center().y();
    const qreal rx = RadiusX();
    const qreal ry = RadiusY();

    points.reserve(count + 1);
    for (int i = 0; i <= count; ++i) {
        const qreal t = qreal(i) / qreal(count);
        // Qt 角度：逆时针为正，故 y 取负号。
        const qreal rad = qDegreesToRadians(startDeg + sweep * t);
        points.append(QPointF(cx + rx * qCos(rad), cy - ry * qSin(rad)));
    }
    return points;
}
