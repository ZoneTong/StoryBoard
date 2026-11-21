#include "LineArtContourProcessor.h"

#include <QLineF>
#include <QPair>
#include <QSet>
#include <QtMath>

namespace {

// 8 邻域，从“右”开始顺时针排列。
const int DIR_X[8] = {1, 1, 0, -1, -1, -1, 0, 1};
const int DIR_Y[8] = {0, 1, 1, 1, 0, -1, -1, -1};

int directionIndex(int dx, int dy)
{
    for (int i = 0; i < 8; ++i) {
        if (DIR_X[i] == dx && DIR_Y[i] == dy) {
            return i;
        }
    }
    return -1;
}

qreal pointToSegmentDistance(const QPointF& point, const QPointF& start, const QPointF& end)
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

} // namespace

namespace LineArtInternal {

QVector<QPointF> SimplifyContour(const QVector<QPointF>& points, qreal tolerance)
{
    if (points.size() <= 2 || tolerance <= 0.0) {
        return points;
    }

    QVector<bool> keep(points.size(), false);
    keep.first() = true;
    keep.last() = true;

    QVector<QPair<int, int>> pending;
    pending.append(qMakePair(0, points.size() - 1));
    while (!pending.isEmpty()) {
        const QPair<int, int> span = pending.takeLast();
        const int first = span.first;
        const int last = span.second;
        if (last <= first + 1) {
            continue;
        }

        qreal maxDistance = -1.0;
        int maxIndex = -1;
        for (int i = first + 1; i < last; ++i) {
            const qreal distance = pointToSegmentDistance(points.at(i), points.at(first), points.at(last));
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }
        if (maxDistance > tolerance && maxIndex > first) {
            keep[maxIndex] = true;
            pending.append(qMakePair(first, maxIndex));
            pending.append(qMakePair(maxIndex, last));
        }
    }

    QVector<QPointF> result;
    result.reserve(points.size());
    for (int i = 0; i < points.size(); ++i) {
        if (keep.at(i)) {
            result.append(points.at(i));
        }
    }
    return result;
}

QVector<QPointF> TraceContour(const QVector<int>& labels, int width, int height, int seed, int regionId)
{
    QVector<QPointF> contour;
    const int startX = seed % width;
    const int startY = seed / width;

    const auto isRegion = [&labels, width, height, regionId](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height && labels.at(y * width + x) == regionId;
    };

    int cx = startX;
    int cy = startY;
    // 起点是这一块最左上的像素，它左边一定不是本块（也可能是图像外）。
    int backX = startX - 1;
    int backY = startY;
    QSet<qint64> visitedStates;
    const int guard = width * height + 64;
    for (int step = 0; step < guard; ++step) {
        // backtrack 相对当前像素的方位，扫描从这里开始顺时针绕一圈。
        int backDir = directionIndex(backX - cx, backY - cy);
        if (backDir < 0) {
            backDir = 4;
        }

        // 同一个像素、又是从同一方向进来的：这一圈已经走完了。
        const qint64 state = (static_cast<qint64>(cy) * width + cx) * 8 + backDir;
        if (visitedStates.contains(state)) {
            break;
        }
        visitedStates.insert(state);

        int nextDir = -1;
        for (int i = 0; i < 8; ++i) {
            const int d = (backDir + i) % 8;
            const int nx = cx + DIR_X[d];
            const int ny = cy + DIR_Y[d];
            if (isRegion(nx, ny)) {
                nextDir = d;
                break;
            }
            // 这一格不是本色块，它就是下一轮扫描的 backtrack。
            backX = nx;
            backY = ny;
        }
        if (nextDir < 0) {
            break;
        }

        contour.append(QPointF(cx, cy));
        cx += DIR_X[nextDir];
        cy += DIR_Y[nextDir];
    }
    return contour;
}

QVector<QPointF> BuildOutlinePoints(const QVector<QPointF>& contour, int area, const LineArtConverter::Options& options)
{
    if (contour.size() < 3) {
        return {};
    }

    // 轮廓周长 ≈ 走一圈的像素步数；2*面积/周长 就是这一块的平均宽度。
    // 平均宽度太小的条带基本是抗锯齿过渡带，描出来只是一圈没意义的细环。
    const qreal thickness = 2.0 * area / qMax(1, contour.size());
    if (options.minRegionThickness > 0.0 && thickness < options.minRegionThickness) {
        return {};
    }

    QVector<QPointF> points = SimplifyContour(contour, options.simplifyTolerance);
    if (points.size() < 2) {
        return {};
    }
    // 补上闭合点，让笔画图元自己形成一个完整的回路。
    if (points.constFirst() != points.constLast()) {
        points.append(points.constFirst());
    }
    if (points.size() < 4) {
        return {};
    }
    return points;
}

} // namespace LineArtInternal
