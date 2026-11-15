#include "CurveGeometry.h"

#include "StrokeSmoothing.h"

#include <QLineF>
#include <QPolygonF>
#include <algorithm>

namespace CurveGeometry {

namespace {

CurveSegment straightSegment(const QPointF& from, const QPointF& to)
{
    const QPointF delta = to - from;
    return CurveSegment{from, from + delta / 3.0, from + delta * 2.0 / 3.0, to};
}

QPointF catmullControl(const QPointF& previous, const QPointF& current, const QPointF& next, qreal sign)
{
    return current + (next - previous) * (sign / 6.0);
}

} // namespace

QVector<CurveSegment> fitSegments(QVector<QPointF> points, qreal tolerance, bool linear)
{
    QVector<CurveSegment> segments;
    if (points.size() < 2) {
        return segments;
    }

    qreal chordLength = 0.0;
    if (tolerance > 0.0 && !linear) {
        // 明显是手抖画出来的直线时直接收成首尾两点，避免留下小波浪。
        if (StrokeSmoothing::looksLikeStraightLine(points, qMax<qreal>(1.0, tolerance), chordLength)) {
            points = {points.constFirst(), points.constLast()};
            linear = true;
        } else {
            points = StrokeSmoothing::simplifyRdp(points, qMax<qreal>(1.0, tolerance));
        }
    }

    if (points.size() < 2) {
        return segments;
    }

    segments.reserve(points.size() - 1);
    if (linear || points.size() == 2) {
        for (int i = 1; i < points.size(); ++i) {
            segments.append(straightSegment(points.at(i - 1), points.at(i)));
        }
        return segments;
    }

    // Catmull-Rom 转三次贝塞尔：端点重复一次，保证首尾切线自然。
    for (int i = 0; i < points.size() - 1; ++i) {
        const QPointF& p0 = points.at(i);
        const QPointF& p1 = points.at(i + 1);
        const QPointF& previous = i > 0 ? points.at(i - 1) : p0;
        const QPointF& next = i + 2 < points.size() ? points.at(i + 2) : p1;
        segments.append(CurveSegment{p0, catmullControl(previous, p0, p1, 1.0), catmullControl(p0, p1, next, -1.0),
                                     p1});
    }
    return segments;
}

QPainterPath buildPath(const QVector<CurveSegment>& segments)
{
    QPainterPath path;
    if (segments.isEmpty()) {
        return path;
    }
    path.moveTo(segments.constFirst().p0);
    for (const CurveSegment& segment : segments) {
        path.cubicTo(segment.c1, segment.c2, segment.p1);
    }
    return path;
}

QPainterPath buildDotPath(const QPointF& center, qreal radius)
{
    QPainterPath path;
    path.addEllipse(center, qMax<qreal>(0.5, radius), qMax<qreal>(0.5, radius));
    return path;
}

QVector<QPointF> knots(const QVector<CurveSegment>& segments)
{
    QVector<QPointF> result;
    if (segments.isEmpty()) {
        return result;
    }
    result.reserve(segments.size() + 1);
    result.append(segments.constFirst().p0);
    for (const CurveSegment& segment : segments) {
        result.append(segment.p1);
    }
    return result;
}

QVector<QPointF> flatten(const QPainterPath& path)
{
    QVector<QPointF> result;
    const QList<QPolygonF> polygons = path.toSubpathPolygons();
    for (const QPolygonF& polygon : polygons) {
        for (const QPointF& point : polygon) {
            if (result.isEmpty() || QLineF(result.constLast(), point).length() > 0.01) {
                result.append(point);
            }
        }
    }
    return result;
}

} // namespace CurveGeometry

bool CurveSegment::operator==(const CurveSegment& other) const
{
    return StrokeSmoothing::samePoint(p0, other.p0) && StrokeSmoothing::samePoint(c1, other.c1) &&
           StrokeSmoothing::samePoint(c2, other.c2) && StrokeSmoothing::samePoint(p1, other.p1);
}
