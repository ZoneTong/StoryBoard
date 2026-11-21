#include "StrokeSmoothing.h"

#include <QLineF>
#include <QtMath>
#include <functional>

namespace StrokeSmoothing {

bool SamePoint(const QPointF& first, const QPointF& second)
{
    return qAbs(first.x() - second.x()) < STATE_EPSILON && qAbs(first.y() - second.y()) < STATE_EPSILON;
}

bool SamePoints(const QVector<QPointF>& first, const QVector<QPointF>& second)
{
    if (first.size() != second.size()) {
        return false;
    }
    for (int i = 0; i < first.size(); ++i) {
        if (!SamePoint(first.at(i), second.at(i))) {
            return false;
        }
    }
    return true;
}

qreal PointToSegmentDistance(const QPointF& point, const QPointF& start, const QPointF& end)
{
    const QPointF segment = end - start;
    const qreal lengthSquared = QPointF::dotProduct(segment, segment);
    if (lengthSquared <= 0.0) {
        return QLineF(point, start).length();
    }

    qreal t = QPointF::dotProduct(point - start, segment) / lengthSquared;
    t = qBound<qreal>(0.0, t, 1.0);
    return QLineF(point, start + segment * t).length();
}

QVector<QPointF> SimplifyRdp(const QVector<QPointF>& points, qreal tolerance)
{
    if (points.size() <= 2 || tolerance <= 0.0) {
        return points;
    }

    QVector<QPointF> result;
    result.reserve(points.size());
    std::function<void(int, int)> simplify = [&](int first, int last) {
        if (last <= first + 1) {
            result.append(points.at(first));
            return;
        }
        qreal maxDistance = 0.0;
        int maxIndex = -1;
        for (int i = first + 1; i < last; ++i) {
            const qreal distance = PointToSegmentDistance(points.at(i), points.at(first), points.at(last));
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }
        if (maxDistance > tolerance && maxIndex > first) {
            simplify(first, maxIndex);
            simplify(maxIndex, last);
        } else {
            result.append(points.at(first));
        }
    };
    simplify(0, points.size() - 1);
    result.append(points.constLast());
    return result;
}

QVector<QPointF> ChaikinSmooth(const QVector<QPointF>& points, int iterations)
{
    QVector<QPointF> result = points;
    for (int pass = 0; pass < iterations && result.size() > 2; ++pass) {
        QVector<QPointF> next;
        next.reserve(result.size() * 2);
        next.append(result.constFirst());
        for (int i = 0; i < result.size() - 1; ++i) {
            const QPointF first = result.at(i);
            const QPointF second = result.at(i + 1);
            next.append((first * 3.0 + second) / 4.0);
            next.append((first + second * 3.0) / 4.0);
        }
        next.append(result.constLast());
        result = next;
    }
    return result;
}

bool LooksLikeStraightLine(const QVector<QPointF>& points, qreal tolerance, qreal& chordLength)
{
    if (points.size() < 3) {
        return true;
    }

    const QPointF start = points.constFirst();
    const QPointF end = points.constLast();
    chordLength = QLineF(start, end).length();
    // 太短的笔画不做“直线吸附”，否则会把一个小勾/小弯折压成点或短线段。
    if (chordLength < qMax<qreal>(16.0, tolerance * 4.0)) {
        return false;
    }

    qreal maxDeviation = 0.0;
    for (int i = 1; i < points.size() - 1; ++i) {
        maxDeviation = qMax(maxDeviation, PointToSegmentDistance(points.at(i), start, end));
    }
    if (maxDeviation > tolerance) {
        return false;
    }

    qreal length = 0.0;
    for (int i = 1; i < points.size(); ++i) {
        length += QLineF(points.at(i - 1), points.at(i)).length();
    }
    // 高频来回抖动的笔画即使最大偏离不大，也不应当作直线吸附。
    return length <= chordLength * 1.6;
}

} // namespace StrokeSmoothing
