#include "PathRegions.h"

#include <QPolygonF>

namespace {

// 一个环是否真的把另一个环包住：只取有限个顶点做包含测试，环的顶点数可能很大，
// 但布尔运算的结果里，孔洞与它的外轮廓共享的顶点不会太多，抽样足够判嵌套。
constexpr int kMaxProbePoints = 24;

bool ringContains(const QPainterPath& ring, const QPolygonF& points)
{
    const int step = qMax(1, points.size() / kMaxProbePoints);
    for (int i = 0; i < points.size(); i += step) {
        if (ring.contains(points.at(i))) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace PathRegions {

QList<QPainterPath> splitDisjoint(const QPainterPath& path)
{
    QList<QPainterPath> pieces;
    if (path.isEmpty()) {
        return pieces;
    }
    const QList<QPolygonF> polygons = path.toSubpathPolygons();
    if (polygons.size() <= 1) {
        pieces.append(path);
        return pieces;
    }

    QList<QPolygonF> rings;
    QList<QPainterPath> ringPaths;
    for (const QPolygonF& polygon : polygons) {
        if (polygon.size() < 3) {
            continue;
        }
        QPainterPath ring;
        ring.setFillRule(Qt::OddEvenFill);
        ring.addPolygon(polygon);
        ring.closeSubpath();
        if (ring.isEmpty()) {
            continue;
        }
        rings.append(polygon);
        ringPaths.append(ring);
    }
    if (ringPaths.size() <= 1) {
        pieces.append(path);
        return pieces;
    }

    // 嵌套深度 = 被多少个其他环包住。偶数层是独立区域的外轮廓，奇数层是它的孔洞：
    // 布尔运算的孔洞总是单独成为一个子路径，方向与填充规则不保证对外可见，只能靠嵌套判。
    QList<int> depths;
    depths.reserve(ringPaths.size());
    for (int i = 0; i < ringPaths.size(); ++i) {
        int depth = 0;
        const QRectF box = rings.at(i).boundingRect();
        for (int j = 0; j < ringPaths.size(); ++j) {
            if (i == j) {
                continue;
            }
            // 包围盒不相交的直接跳过，先挡掉绝大多数无关环。
            if (!ringPaths.at(j).boundingRect().contains(box)) {
                continue;
            }
            if (ringContains(ringPaths.at(j), rings.at(i))) {
                ++depth;
            }
        }
        depths.append(depth);
    }

    for (int i = 0; i < ringPaths.size(); ++i) {
        if (depths.at(i) % 2 != 0) {
            continue; // 孔洞跟着它的外轮廓走，不单独成块
        }
        QPainterPath piece = ringPaths.at(i);
        // 直接嵌在它里面的孔洞挖掉；更深一层的岛属于别的块，由那一层自己处理。
        for (int j = 0; j < ringPaths.size(); ++j) {
            if (i == j || depths.at(j) != depths.at(i) + 1) {
                continue;
            }
            if (!ringPaths.at(i).boundingRect().contains(rings.at(j).boundingRect())) {
                continue;
            }
            if (ringContains(ringPaths.at(i), rings.at(j))) {
                piece = piece.subtracted(ringPaths.at(j));
            }
        }
        piece.setFillRule(Qt::OddEvenFill);
        if (!piece.isEmpty()) {
            pieces.append(piece);
        }
    }
    if (pieces.isEmpty()) {
        pieces.append(path);
    }
    return pieces;
}

qreal area(const QPainterPath& path)
{
    // toFillPolygons 已按填充规则把外轮廓与孔洞解成互不重叠的多边形，
    // 各环鞋带公式面积取绝对值求和即填充面积。
    qreal total = 0.0;
    const QList<QPolygonF> polygons = path.toFillPolygons();
    for (const QPolygonF& polygon : polygons) {
        const int count = polygon.size();
        if (count < 3) {
            continue;
        }
        qreal sum = 0.0;
        for (int i = 0; i < count; ++i) {
            const QPointF& a = polygon.at(i);
            const QPointF& b = polygon.at((i + 1) % count);
            sum += a.x() * b.y() - b.x() * a.y();
        }
        total += qAbs(sum) / 2.0;
    }
    return total;
}

} // namespace PathRegions
