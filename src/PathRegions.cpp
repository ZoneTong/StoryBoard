#include "PathRegions.h"

#include <QLineF>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <limits>

namespace {

// 一个环是否真的把另一个环包住：只取有限个顶点做包含测试，环的顶点数可能很大，
// 但布尔运算的结果里，孔洞与它的外轮廓共享的顶点不会太多，抽样足够判嵌套。
constexpr int MAX_PROBE_POINTS = 24;
// 面积判定容差（px²）：布尔减法在近切线处会留下抗锯齿细缝，小于该值视为空。
constexpr qreal AREA_EPS = 0.05;
// 共享边探测带宽度（px）：把另一区域轮廓描成细带，判断本区域轮廓是否落在带内。
constexpr qreal CONTACT_BAND_WIDTH = 1.0;
// 接触链长度阈值：仅角点接触时接触链只是一个小折角，长度低于该值。
constexpr qreal CONTACT_LENGTH_EPS = 1.5;

bool RingContains(const QPainterPath& ring, const QPolygonF& points)
{
    const int step = qMax(1, points.size() / MAX_PROBE_POINTS);
    for (int i = 0; i < points.size(); i += step) {
        if (ring.contains(points.at(i))) {
            return true;
        }
    }
    return false;
}

// 多边形闭合折线的周长（边长按鞋带边逐项求和），用于近似接触链长度。
qreal PolylineLength(const QPolygonF& polygon)
{
    const int count = polygon.size();
    if (count < 2) {
        return 0.0;
    }
    qreal length = 0.0;
    for (int i = 0; i < count; ++i) {
        length += QLineF(polygon.at(i), polygon.at((i + 1) % count)).length();
    }
    return length;
}

// 路径轮廓接触链长度：各子路径周长求和。
qreal ContactLength(const QPainterPath& path)
{
    qreal total = 0.0;
    const QList<QPolygonF> polygons = path.toSubpathPolygons();
    for (const QPolygonF& polygon : polygons) {
        total += PolylineLength(polygon);
    }
    return total;
}

} // namespace

namespace PathRegions {

QList<QPainterPath> SplitDisjoint(const QPainterPath& path)
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
            if (RingContains(ringPaths.at(j), rings.at(i))) {
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
            if (RingContains(ringPaths.at(i), rings.at(j))) {
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
        return pieces;
    }

    // 全覆盖不变量（§5.2）：各块并集必须兜住原填充区域。拆分一旦丢面积（嵌套误判、
    // 角点接触被并到同一块等），退回整块返回，绝不允许产出缺角的结果。
    QPainterPath unionPieces;
    for (const QPainterPath& piece : std::as_const(pieces)) {
        unionPieces = unionPieces.isEmpty() ? piece : unionPieces.united(piece);
    }
    const qreal originalArea = Area(path);
    const qreal coverageEps = qMax(1.0, originalArea * 0.01);
    if (Area(unionPieces) < originalArea - coverageEps) {
        return {path};
    }
    return pieces;
}

qreal Area(const QPainterPath& path)
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

bool RegionsContinuous(const QPainterPath& a, const QPainterPath& b)
{
    if (a.isEmpty() || b.isEmpty()) {
        return false;
    }
    // 实际重叠：交集面积 > 0 即连续。
    if (Area(a.intersected(b)) > AREA_EPS) {
        return true;
    }
    // 共享边：把 b 轮廓描成细带，a 落在带内的接触链长度超过阈值才算共边；
    // 仅角点接触时接触链只是一个小折角，长度趋近 0，判为不连续。
    QPainterPathStroker stroker;
    stroker.setWidth(CONTACT_BAND_WIDTH);
    const QPainterPath bandB = stroker.createStroke(b);
    return ContactLength(a.intersected(bandB)) > CONTACT_LENGTH_EPS;
}

qreal SharedBoundaryLength(const QPainterPath& a, const QPainterPath& b)
{
    if (a.isEmpty() || b.isEmpty()) {
        return 0.0;
    }
    const QPainterPath overlap = a.intersected(b);
    if (Area(overlap) > AREA_EPS) {
        // 相互重叠时共享边界取交集轮廓周长作为近似。
        return ContactLength(overlap);
    }
    QPainterPathStroker stroker;
    stroker.setWidth(CONTACT_BAND_WIDTH);
    const QPainterPath bandB = stroker.createStroke(b);
    return ContactLength(a.intersected(bandB));
}

qreal RegionDistance(const QPainterPath& a, const QPainterPath& b)
{
    if (a.isEmpty() || b.isEmpty()) {
        return std::numeric_limits<qreal>::max();
    }
    return QLineF(a.boundingRect().center(), b.boundingRect().center()).length();
}

} // namespace PathRegions
