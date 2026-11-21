#include "LineSplitOps.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "LineItemAbstract.h"
#include "PathRegions.h"
#include "PolygonItem.h"
#include "ShapeItem.h"

#include <QPainterPath>
#include <QSet>
#include <QUndoStack>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

// 需求 25：刀线条带与波面等弯曲边界近切线相交时，布尔减法会留下面积趋近 0 的
// 退化碎屑（细环、纸屑），单独成图元后渲染成孤立的“奇怪色块”。碎片填充面积
// 低于该阈值视为碎屑直接丢弃；约 4×4px，肉眼可辨的最小碎块远大于此。
constexpr qreal MIN_FRAGMENT_AREA = 16.0;

// 刀线的几何描述：两端点与端点处的切向单位向量。
struct Knife {
    QPointF start;
    QPointF end;
    QPointF startDir;
    QPointF endDir;
};

bool isFillTarget(CanvasItem* item)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->ShapeLayerCount() == 0) {
        return true;
    }
    if (auto* composite = dynamic_cast<ShapeItem*>(item); composite && composite->ShapeLayerCount() > 0) {
        return composite->HasShapeFillArea();
    }
    return false;
}

bool isKnifeItem(CanvasItem* item)
{
    if (dynamic_cast<LineItemAbstract*>(item)) {
        return true;
    }
    return false;
}

// 从路径提取两端点与端点切向：首元素为起点，末元素为终点；
// 切向取相邻元素连线方向，三次曲线用控制点近似，方向退化为零时报无效。
bool knifeFromPath(const QPainterPath& path, Knife* out)
{
    const int count = path.elementCount();
    if (count < 2) {
        return false;
    }
    const QPainterPath::Element first = path.elementAt(0);
    const QPainterPath::Element second = path.elementAt(1);
    const QPainterPath::Element last = path.elementAt(count - 1);
    const QPainterPath::Element previous = path.elementAt(count - 2);
    const QPointF startDir(second.x - first.x, second.y - first.y);
    const QPointF endDir(last.x - previous.x, last.y - previous.y);
    if (qFuzzyIsNull(startDir.x()) && qFuzzyIsNull(startDir.y())) {
        return false;
    }
    if (qFuzzyIsNull(endDir.x()) && qFuzzyIsNull(endDir.y())) {
        return false;
    }
    out->start = QPointF(first.x, first.y);
    out->end = QPointF(last.x, last.y);
    out->startDir = startDir / qSqrt(QPointF::dotProduct(startDir, startDir));
    out->endDir = endDir / qSqrt(QPointF::dotProduct(endDir, endDir));
    return true;
}

// 收集切刀几何（场景坐标）：直线图元取 outlineShape 的线段，曲线取中心线。
// 需求 21：曲线同时返回完整轨迹路径，供按轨迹切分使用。
bool collectKnife(CanvasItem* item, Knife* out, QPainterPath* trajectoryPath)
{
    if (auto* curve = dynamic_cast<LineItemAbstract*>(item)) {
        const QPainterPath scenePath = curve->mapToScene(curve->OutlineShape());
        if (trajectoryPath) {
            *trajectoryPath = scenePath;
        }
        return knifeFromPath(scenePath, out);
    }
    if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->ShapeLayerCount() == 0) {
        const QPainterPath scenePath = shape->mapToScene(shape->OutlineShape());
        if (trajectoryPath) {
            *trajectoryPath = scenePath;
        }
        return knifeFromPath(scenePath, out);
    }
    return false;
}

// 求射线与矩形边界的交点：从 origin 沿 dir 走，返回最先离开矩形的边界点。
QPointF rayBoundaryPoint(const QRectF& rect, const QPointF& origin, const QPointF& dir)
{
    qreal best = std::numeric_limits<qreal>::max();
    QPointF result = origin;
    auto consider = [&best, &result](qreal t, const QPointF& point) {
        if (t > 0.0 && t < best) {
            best = t;
            result = point;
        }
    };
    if (!qFuzzyIsNull(dir.x())) {
        consider((rect.left() - origin.x()) / dir.x(), QPointF(rect.left(), origin.y()));
        consider((rect.right() - origin.x()) / dir.x(), QPointF(rect.right(), origin.y()));
    }
    if (!qFuzzyIsNull(dir.y())) {
        consider((rect.top() - origin.y()) / dir.y(), QPointF(origin.x(), rect.top()));
        consider((rect.bottom() - origin.y()) / dir.y(), QPointF(origin.x(), rect.bottom()));
    }
    return result;
}

// 边界点在顺时针边界上的位置参数：上边 [0,1) → 右边 [1,2) → 下边 [2,3) → 左边 [3,4)。
qreal boundaryKey(const QRectF& rect, const QPointF& point)
{
    const qreal w = qMax<qreal>(rect.width(), 1.0);
    const qreal h = qMax<qreal>(rect.height(), 1.0);
    if (qFuzzyCompare(point.y(), rect.top())) {
        return (point.x() - rect.left()) / w;
    }
    if (qFuzzyCompare(point.x(), rect.right())) {
        return 1.0 + (point.y() - rect.top()) / h;
    }
    if (qFuzzyCompare(point.y(), rect.bottom())) {
        return 2.0 + (rect.right() - point.x()) / w;
    }
    return 3.0 + (rect.bottom() - point.y()) / h;
}

// 把两个边界点按序插入矩形角点环，取 entry→exit（顺时针）或反向的闭合折线。
QList<QPointF> perimeterChain(const QRectF& rect, const QPointF& entry, const QPointF& exit, bool fromEntry)
{
    const QList<QPointF> corners = {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
    QList<QPair<qreal, QPointF>> ring;
    for (const QPointF& corner : corners) {
        ring.append({boundaryKey(rect, corner), corner});
    }
    ring.append({boundaryKey(rect, entry), entry});
    ring.append({boundaryKey(rect, exit), exit});
    std::stable_sort(
        ring.begin(), ring.end(),
        [](const QPair<qreal, QPointF>& left, const QPair<qreal, QPointF>& right) { return left.first < right.first; });
    QList<QPointF> ordered;
    int startIndex = -1;
    int stopIndex = -1;
    for (int i = 0; i < ring.size(); ++i) {
        if (ring.at(i).second == entry) {
            startIndex = i;
        }
        if (ring.at(i).second == exit) {
            stopIndex = i;
        }
        ordered.append(ring.at(i).second);
    }
    if (startIndex < 0 || stopIndex < 0) {
        return {};
    }
    if (!fromEntry) {
        std::swap(startIndex, stopIndex);
    }
    QList<QPointF> chain;
    const int size = ordered.size();
    for (int step = 0; step < size; ++step) {
        chain.append(ordered.at((startIndex + step) % size));
        if ((startIndex + step) % size == stopIndex) {
            break;
        }
    }
    return chain;
}

QPainterPath polygonPath(const QList<QPointF>& points)
{
    QPainterPath path;
    if (points.size() < 3) {
        return path;
    }
    path.moveTo(points.constFirst());
    for (int i = 1; i < points.size(); ++i) {
        path.lineTo(points.at(i));
    }
    path.closeSubpath();
    return path;
}

// 刀线两端沿端点切向延伸到画布边界后，画布被分成两个闭合区域。
QList<QPainterPath> canvasHalfRegions(const QRectF& canvas, const Knife& knife)
{
    const QPointF entry = rayBoundaryPoint(canvas, knife.start, -knife.startDir);
    const QPointF exit = rayBoundaryPoint(canvas, knife.end, knife.endDir);
    if (entry == knife.start || exit == knife.end) {
        return {};
    }
    QList<QPainterPath> regions;
    regions.append(polygonPath(perimeterChain(canvas, entry, exit, true)));
    regions.append(polygonPath(perimeterChain(canvas, entry, exit, false)));
    return regions;
}

// 被切图元的填充轮廓（场景坐标）。
QPainterPath fillPathOf(CanvasItem* item)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->ShapeLayerCount() == 0) {
        return shape->mapToScene(shape->OutlineShape());
    }
    if (auto* composite = dynamic_cast<ShapeItem*>(item); composite && composite->ShapeLayerCount() > 0) {
        QPainterPath fill;
        for (int index = 0; index < composite->ShapeLayerCount(); ++index) {
            const ShapeLayer layer = composite->shapeLayer(index);
            if (layer.role != ShapeLayerRole::FILL_AREA || layer.path.isEmpty()) {
                continue;
            }
            const QPainterPath part = composite->mapToScene(composite->MapShapeLayerPath(layer));
            fill = fill.isEmpty() ? part : fill.united(part);
        }
        return fill;
    }
    return QPainterPath();
}

} // namespace

namespace LineSplitOps {

bool supports(CanvasScene* scene)
{
    if (!scene) {
        return false;
    }
    int fills = 0;
    int knives = 0;
    for (CanvasItem* item : scene->selectionOrder()) {
        if (!item || !scene->isItemEditable(item)) {
            continue;
        }
        if (isFillTarget(item)) {
            ++fills;
        } else if (isKnifeItem(item)) {
            ++knives;
        }
    }
    return fills >= 1 && knives == 1;
}

bool apply(CanvasScene* scene, const QString& commandText)
{
    if (!scene) {
        return false;
    }
    Knife knife;
    CanvasItem* knifeItem = nullptr;
    QPainterPath knifeTrajectory;
    bool trajectoryKnife = false;
    struct Target {
        CanvasItem* item = nullptr;
        QPainterPath fill;
    };
    QList<Target> targets;
    for (CanvasItem* item : scene->selectionOrder()) {
        if (!item || !scene->isItemEditable(item)) {
            continue;
        }
        if (isFillTarget(item)) {
            Target target;
            target.item = item;
            target.fill = fillPathOf(item);
            if (!target.fill.isEmpty()) {
                targets.append(target);
            }
        } else if (isKnifeItem(item) && !knifeItem && collectKnife(item, &knife, &knifeTrajectory)) {
            knifeItem = item;
            // 刀线全部是曲线族：弧线、正弦的轨迹是弯的，统一按轨迹条带切。
            trajectoryKnife = true;
        }
    }
    if (targets.isEmpty() || !knifeItem) {
        return false;
    }
    // 需求 21：直线刀线沿用"端点切向延伸到画布边界"的半区切法，切口是笔直
    // 的直线；曲线刀线按完整轨迹切——沿刀线中心线取一条与刀线同宽的条带，
    // 从被切填充里挖掉条带后按互不相连拆块，切口贴合曲线走向。
    QList<QPainterPath> regions;
    QPainterPath knifeBand;
    if (trajectoryKnife) {
        auto* curveKnife = dynamic_cast<LineItemAbstract*>(knifeItem);
        auto* shapeKnife = dynamic_cast<ShapeItem*>(knifeItem);
        QPainterPathStroker stroker;
        stroker.setWidth(
            qMax<qreal>(1.0, curveKnife ? curveKnife->StrokeWidth() : (shapeKnife ? shapeKnife->StrokeWidth() : 1.0)));
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        knifeBand = stroker.createStroke(knifeTrajectory);
        if (knifeBand.isEmpty()) {
            return false;
        }
    } else {
        regions = canvasHalfRegions(scene->canvasRect(), knife);
        if (regions.isEmpty()) {
            return false;
        }
    }

    QUndoStack* undo = scene->undoStack();
    if (!undo) {
        return false;
    }

    // 先离线算出全部碎片。直线刀线：每个被切图元的两块画布半区交集再按
    // 互不相连拆块；曲线刀线：从填充里挖掉刀线条带后按互不相连拆块。
    // 碎片样式沿用被切图元自身的填充与描线。刀线必须真的穿过填充图形
    // （曲线至少拆出两块；直线两块半区交集都非空）才算切到，整块落在
    // 刀线一侧的图元原样保留。
    struct Fragment {
        CanvasItem* source = nullptr;
        QPainterPath piece;
    };
    QList<Fragment> fragments;
    for (const Target& target : targets) {
        QList<QPainterPath> pieces;
        bool straddles = true;
        if (trajectoryKnife) {
            pieces = PathRegions::splitDisjoint(target.fill.subtracted(knifeBand));
            // 需求 25：先丢弃退化碎屑，剩下的块数才是真实的切割块数。
            pieces.removeIf([](const QPainterPath& piece) { return PathRegions::area(piece) < MIN_FRAGMENT_AREA; });
            if (pieces.size() < 2) {
                straddles = false;
            }
        } else {
            for (const QPainterPath& region : regions) {
                QList<QPainterPath> parts = PathRegions::splitDisjoint(region.intersected(target.fill));
                parts.removeIf([](const QPainterPath& piece) { return PathRegions::area(piece) < MIN_FRAGMENT_AREA; });
                if (parts.isEmpty()) {
                    straddles = false;
                    break;
                }
                pieces.append(parts);
            }
        }
        if (!straddles) {
            continue;
        }
        for (const QPainterPath& piece : pieces) {
            fragments.append(Fragment{target.item, piece});
        }
    }
    if (fragments.isEmpty()) {
        return false;
    }

    QSet<CanvasItem*> removedSet;
    QList<CanvasItem*> removed;
    for (const Fragment& fragment : fragments) {
        if (!removedSet.contains(fragment.source)) {
            removedSet.insert(fragment.source);
            removed.append(fragment.source);
        }
    }

    undo->beginMacro(commandText);
    undo->push(new RemoveItemsCommand(scene, removed, commandText));
    QList<CanvasItem*> created;
    for (const Fragment& fragment : fragments) {
        auto* source = fragment.source;
        auto* shapeSource = dynamic_cast<ShapeItem*>(source);
        shapeSource = shapeSource && shapeSource->ShapeLayerCount() == 0 ? shapeSource : nullptr;
        auto* compositeSource = dynamic_cast<ShapeItem*>(source);
        compositeSource = compositeSource && compositeSource->ShapeLayerCount() > 0 ? compositeSource : shapeSource;
        PolygonItem::Layer layer;
        layer.path = fragment.piece;
        layer.fill = compositeSource->FillColor();
        layer.stroke = compositeSource->StrokeColor();
        layer.strokeWidth = compositeSource->StrokeWidth();
        layer.filled = true;
        layer.role = PolygonItem::LayerRole::FILLED_AREA;
        auto* item = new PolygonItem(QList<PolygonItem::Layer>{layer}, compositeSource->fillStyle(),
                                     compositeSource->StrokeStyle());
        item->SetName(scene->uniqueItemName(item->TypeDisplayName()));
        item->SetLayerId(source->LayerId());
        item->SetZOrder(scene->nextHeight(source->LayerId()));
        undo->push(new AddItemCommand(scene, item, commandText));
        created.append(item);
    }
    undo->endMacro();
    scene->clearSelection();
    knifeItem->setSelected(true);
    for (CanvasItem* fragment : created) {
        fragment->setSelected(true);
    }
    return true;
}

} // namespace LineSplitOps
