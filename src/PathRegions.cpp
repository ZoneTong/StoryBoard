#include "PathRegions.h"

#include <QLineF>
#include <QMap>
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

// 单个环信息：poly 为展平多边形，ring 为独立填充路径；depth 为被其他环包住的层数
// （偶数层是独立区域的外轮廓，奇数层是孔洞），parent 为直接包住它的最内层环下标。
struct RingInfo {
    QPolygonF poly;
    QPainterPath ring;
    int depth = 0;
    int parent = -1;
};

// 并查集根查找（路径压缩）。
int FindRingRoot(QList<int>& roots, int index)
{
    while (roots.at(index) != index) {
        roots[index] = roots.at(roots.at(index));
        index = roots.at(index);
    }
    return index;
}

// toFillPolygons 会把孔洞以“同向闭合环 + 零宽桥”的形式内联进同一个多边形，
// 走向不归一导致鞋带面积无法自减；在重复顶点处切开，还原出一个个独立闭合环。
QList<QPolygonF> SplitRingLoops(const QPolygonF& polygon)
{
    QList<QPolygonF> loops;
    QPolygonF current;
    for (const QPointF& point : polygon) {
        const int index = current.indexOf(point);
        if (index >= 0) {
            loops.append(current.mid(index));
            current = current.mid(0, index);
        } else {
            current.append(point);
        }
    }
    if (current.size() >= 3) {
        loops.append(current);
    }
    return loops;
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

    QList<RingInfo> rings;
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
        RingInfo info;
        info.poly = polygon;
        info.ring = ring;
        rings.append(info);
    }
    if (rings.size() <= 1) {
        pieces.append(path);
        return pieces;
    }

    const int ringCount = rings.size();
    // 第一遍：嵌套深度 = 被多少个其他环包住。
    for (int i = 0; i < ringCount; ++i) {
        const QRectF box = rings.at(i).poly.boundingRect();
        for (int j = 0; j < ringCount; ++j) {
            if (i == j || !rings.at(j).ring.boundingRect().contains(box)) {
                continue;
            }
            if (RingContains(rings.at(j).ring, rings.at(i).poly)) {
                ++rings[i].depth;
            }
        }
    }
    // 第二遍：直接父环 = 包住它的环中最内层（深度最大）的那个。
    for (int i = 0; i < ringCount; ++i) {
        const QRectF box = rings.at(i).poly.boundingRect();
        for (int j = 0; j < ringCount; ++j) {
            if (i == j || rings.at(j).depth >= rings.at(i).depth) {
                continue;
            }
            if (!rings.at(j).ring.boundingRect().contains(box)) {
                continue;
            }
            if (!RingContains(rings.at(j).ring, rings.at(i).poly)) {
                continue;
            }
            if (rings.at(i).parent < 0 || rings.at(j).depth > rings.at(rings.at(i).parent).depth) {
                rings[i].parent = j;
            }
        }
    }

    // 外轮廓环（偶数深度）按“同深度 + 同父环 + 实际接触”归并为同一块：
    // 共边或重叠的子路径属于同一连续填充；隔着孔洞的嵌套岛（如深度 2）不与外层并块。
    QList<int> roots(ringCount);
    for (int i = 0; i < ringCount; ++i) {
        roots[i] = i;
    }
    for (int i = 0; i < ringCount; ++i) {
        if (rings.at(i).depth % 2 != 0) {
            continue;
        }
        for (int j = i + 1; j < ringCount; ++j) {
            if (rings.at(j).depth % 2 != 0 || rings.at(i).depth != rings.at(j).depth ||
                rings.at(i).parent != rings.at(j).parent) {
                continue;
            }
            if (!RegionsContinuous(rings.at(i).ring, rings.at(j).ring)) {
                continue;
            }
            const int rootI = FindRingRoot(roots, i);
            const int rootJ = FindRingRoot(roots, j);
            if (rootI != rootJ) {
                roots[rootJ] = rootI;
            }
        }
    }

    // 每组外环的并集成一块，孔洞（奇数深度）从其父环所在组挖掉。
    QMap<int, QPainterPath> groupPaths;
    for (int i = 0; i < ringCount; ++i) {
        if (rings.at(i).depth % 2 != 0) {
            continue;
        }
        const int root = FindRingRoot(roots, i);
        const QPainterPath& ring = rings.at(i).ring;
        groupPaths[root] = groupPaths.value(root).isEmpty() ? ring : groupPaths.value(root).united(ring);
    }
    for (int i = 0; i < ringCount; ++i) {
        if (rings.at(i).depth % 2 == 0 || rings.at(i).parent < 0 || rings.at(rings.at(i).parent).depth % 2 != 0) {
            continue;
        }
        const int root = FindRingRoot(roots, rings.at(i).parent);
        if (!groupPaths.contains(root)) {
            continue;
        }
        groupPaths[root] = groupPaths.value(root).subtracted(rings.at(i).ring);
    }
    for (auto it = groupPaths.cbegin(); it != groupPaths.cend(); ++it) {
        QPainterPath piece = it.value();
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
    // toFillPolygons 先按填充规则合并同向重叠的子路径，再把孔洞以同向闭合环内联；
    // 切环后按全局嵌套深度奇偶做加减（偶数层外轮廓取正、奇数层孔洞取负）才是真实填充面积。
    QList<QPolygonF> loops;
    const QList<QPolygonF> polygons = path.toFillPolygons();
    for (const QPolygonF& polygon : polygons) {
        loops.append(SplitRingLoops(polygon));
    }
    if (loops.isEmpty()) {
        return 0.0;
    }
    QList<QRectF> boxes;
    boxes.reserve(loops.size());
    for (const QPolygonF& loop : loops) {
        boxes.append(loop.boundingRect());
    }
    qreal total = 0.0;
    for (int i = 0; i < loops.size(); ++i) {
        const QPolygonF& loop = loops.at(i);
        const int count = loop.size();
        if (count < 3) {
            continue;
        }
        qreal sum = 0.0;
        for (int j = 0; j < count; ++j) {
            const QPointF& a = loop.at(j);
            const QPointF& b = loop.at((j + 1) % count);
            sum += a.x() * b.y() - b.x() * a.y();
        }
        const qreal ringArea = qAbs(sum) / 2.0;
        int depth = 0;
        for (int j = 0; j < loops.size(); ++j) {
            if (i == j || !boxes.at(j).contains(boxes.at(i))) {
                continue;
            }
            QPainterPath ring;
            ring.setFillRule(Qt::OddEvenFill);
            ring.addPolygon(loops.at(j));
            ring.closeSubpath();
            if (RingContains(ring, loop)) {
                ++depth;
            }
        }
        total += (depth % 2 == 0) ? ringArea : -ringArea;
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
    // 共享边：把 b 轮廓描成细带，a 落在带内的部分折算回链长（带只有一半在 b 外侧，
    // 面积需除以半带宽）；仅角点接触时接触链只剩带角的小圆斑，链长趋近 0，判为不连续。
    QPainterPathStroker stroker;
    stroker.setWidth(CONTACT_BAND_WIDTH);
    const QPainterPath bandB = stroker.createStroke(b);
    return 2.0 * Area(a.intersected(bandB)) / CONTACT_BAND_WIDTH > CONTACT_LENGTH_EPS;
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
    return 2.0 * Area(a.intersected(bandB)) / CONTACT_BAND_WIDTH;
}

qreal RegionDistance(const QPainterPath& a, const QPainterPath& b)
{
    if (a.isEmpty() || b.isEmpty()) {
        return std::numeric_limits<qreal>::max();
    }
    return QLineF(a.boundingRect().center(), b.boundingRect().center()).length();
}

} // namespace PathRegions
