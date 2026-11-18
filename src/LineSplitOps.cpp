#include "LineSplitOps.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "PathRegions.h"
#include "ShapeItem.h"

#include <QPainterPath>
#include <QSet>
#include <QUndoStack>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

// 刀线的几何描述：两端点与端点处的切向单位向量。
struct Knife {
    QPointF start;
    QPointF end;
    QPointF startDir;
    QPointF endDir;
};

bool isFillTarget(CanvasItem* item)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        return !shapeTypeIsStrokeOnly(shape->shapeType());
    }
    if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
        for (const CompositeShapeItem::Layer& layer : composite->layers()) {
            if (layer.role == CompositeShapeItem::LayerRole::FilledArea && !layer.path.isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

bool isKnifeItem(CanvasItem* item)
{
    if (dynamic_cast<CurveItem*>(item)) {
        return true;
    }
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        return shape->shapeType() == ShapeType::Line;
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
bool collectKnife(CanvasItem* item, Knife* out)
{
    if (auto* curve = dynamic_cast<CurveItem*>(item)) {
        return knifeFromPath(curve->mapToScene(curve->outlineShape()), out);
    }
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        return knifeFromPath(shape->mapToScene(shape->outlineShape()), out);
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
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        return shape->mapToScene(shape->outlineShape());
    }
    if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
        QPainterPath fill;
        for (const CompositeShapeItem::Layer& layer : composite->layers()) {
            if (layer.role == CompositeShapeItem::LayerRole::FilledArea && !layer.path.isEmpty()) {
                const QPainterPath part = composite->mapToScene(composite->mapLayerPath(layer.path));
                fill = fill.isEmpty() ? part : fill.united(part);
            }
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
        } else if (isKnifeItem(item) && !knifeItem && collectKnife(item, &knife)) {
            knifeItem = item;
        }
    }
    if (targets.isEmpty() || !knifeItem) {
        return false;
    }
    const QList<QPainterPath> regions = canvasHalfRegions(scene->canvasRect(), knife);
    if (regions.isEmpty()) {
        return false;
    }

    QUndoStack* undo = scene->undoStack();
    if (!undo) {
        return false;
    }

    // 先离线算出全部碎片：每个被切图元的两块区域交集再按互不相连拆块，
    // 碎片样式沿用被切图元自身的填充与描线。刀线必须真的穿过填充图形
    // （两块区域交集都非空）才算切到，整块落在刀线一侧的图元原样保留。
    struct Fragment {
        CanvasItem* source = nullptr;
        QPainterPath piece;
    };
    QList<Fragment> fragments;
    for (const Target& target : targets) {
        QList<QPainterPath> pieces;
        bool straddles = true;
        for (const QPainterPath& region : regions) {
            const QList<QPainterPath> parts = PathRegions::splitDisjoint(region.intersected(target.fill));
            if (parts.isEmpty()) {
                straddles = false;
                break;
            }
            pieces.append(parts);
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
        auto* compositeSource = dynamic_cast<CompositeShapeItem*>(source);
        CompositeShapeItem::Layer layer;
        layer.path = fragment.piece;
        layer.fill = shapeSource ? shapeSource->fillColor() : compositeSource->fillColor();
        layer.stroke = shapeSource ? shapeSource->strokeColor() : compositeSource->strokeColor();
        layer.strokeWidth = shapeSource ? shapeSource->strokeWidth() : compositeSource->strokeWidth();
        layer.filled = true;
        layer.role = CompositeShapeItem::LayerRole::FilledArea;
        auto* item = new CompositeShapeItem(QList<CompositeShapeItem::Layer>{layer},
                                            shapeSource ? shapeSource->fillStyle() : compositeSource->fillStyle(),
                                            shapeSource ? shapeSource->strokeStyle() : compositeSource->strokeStyle());
        item->setName(scene->uniqueItemName(item->typeDisplayName()));
        item->setLayerId(source->layerId());
        item->setHeight(scene->nextHeight(source->layerId()));
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
