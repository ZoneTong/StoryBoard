#include "LineSplitOps.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "LineAbstractItem.h"
#include "MergeRegions.h"
#include "PathRegions.h"
#include "PolygonItem.h"
#include "ShapeAbstractItem.h"

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

// dynamic_cast 到 PolygonItem 属白名单定点场景（D16）：按内部图层取几何
// 只存在于该叶子类，无法用标量属性表达。
PolygonItem* AsPolygon(CanvasItem* item)
{
    return dynamic_cast<PolygonItem*>(item);
}

bool IsFillTarget(CanvasItem* item)
{
    if (const PolygonItem* polygon = AsPolygon(item)) {
        return polygon->HasFillAreaLayer();
    }
    return dynamic_cast<ShapeAbstractItem*>(item) != nullptr;
}

bool IsKnifeItem(CanvasItem* item)
{
    if (dynamic_cast<LineAbstractItem*>(item)) {
        return true;
    }
    return false;
}

// 从路径提取两端点与端点切向：首元素为起点，末元素为终点；
// 切向取相邻元素连线方向，三次曲线用控制点近似，方向退化为零时报无效。
bool KnifeFromPath(const QPainterPath& path, Knife* out)
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
bool CollectKnife(CanvasItem* item, Knife* out, QPainterPath* trajectoryPath)
{
    if (auto* curve = dynamic_cast<LineAbstractItem*>(item)) {
        const QPainterPath scenePath = curve->mapToScene(curve->OutlineShape());
        if (trajectoryPath) {
            *trajectoryPath = scenePath;
        }
        return KnifeFromPath(scenePath, out);
    }
    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
        // 多边形按内部图层切开（见下方分支），普通闭合图形的轮廓整体当刀线。
        if (AsPolygon(item)) {
            return false;
        }
        const QPainterPath scenePath = shape->mapToScene(shape->OutlineShape());
        if (trajectoryPath) {
            *trajectoryPath = scenePath;
        }
        return KnifeFromPath(scenePath, out);
    }
    return false;
}

// 求射线与矩形边界的交点：从 origin 沿 dir 走，返回最先离开矩形的边界点。
QPointF RayBoundaryPoint(const QRectF& rect, const QPointF& origin, const QPointF& dir)
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
qreal BoundaryKey(const QRectF& rect, const QPointF& point)
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
QList<QPointF> PerimeterChain(const QRectF& rect, const QPointF& entry, const QPointF& exit, bool fromEntry)
{
    const QList<QPointF> corners = {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
    QList<QPair<qreal, QPointF>> ring;
    for (const QPointF& corner : corners) {
        ring.append({BoundaryKey(rect, corner), corner});
    }
    ring.append({BoundaryKey(rect, entry), entry});
    ring.append({BoundaryKey(rect, exit), exit});
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

QPainterPath PolygonPath(const QList<QPointF>& points)
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
QList<QPainterPath> CanvasHalfRegions(const QRectF& canvas, const Knife& knife)
{
    const QPointF entry = RayBoundaryPoint(canvas, knife.start, -knife.startDir);
    const QPointF exit = RayBoundaryPoint(canvas, knife.end, knife.endDir);
    if (entry == knife.start || exit == knife.end) {
        return {};
    }
    QList<QPainterPath> regions;
    regions.append(PolygonPath(PerimeterChain(canvas, entry, exit, true)));
    regions.append(PolygonPath(PerimeterChain(canvas, entry, exit, false)));
    return regions;
}

// 被切图元的填充轮廓（场景坐标）。
QPainterPath FillPathOf(CanvasItem* item)
{
    if (const PolygonItem* polygon = AsPolygon(item)) {
        QPainterPath fill;
        for (const PolygonItem::Layer& layer : polygon->Layers()) {
            if (layer.role != PolygonItem::LayerRole::FILLED_AREA || layer.path.isEmpty()) {
                continue;
            }
            const QPainterPath part = item->mapToScene(polygon->MapLayerPath(layer.path));
            fill = fill.isEmpty() ? part : fill.united(part);
        }
        return fill;
    }
    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(item)) {
        return shape->mapToScene(shape->OutlineShape());
    }
    return QPainterPath();
}

} // namespace

namespace LineSplitOps {

bool Supports(CanvasScene* scene)
{
    if (!scene) {
        return false;
    }
    int fills = 0;
    int knives = 0;
    for (CanvasItem* item : scene->SelectionOrder()) {
        if (!item || !scene->IsItemEditable(item)) {
            continue;
        }
        if (IsFillTarget(item)) {
            ++fills;
        } else if (IsKnifeItem(item)) {
            ++knives;
        }
    }
    return fills >= 1 && knives == 1;
}

bool Apply(CanvasScene* scene, const QString& commandText)
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
    for (CanvasItem* item : scene->SelectionOrder()) {
        if (!item || !scene->IsItemEditable(item)) {
            continue;
        }
        if (IsFillTarget(item)) {
            Target target;
            target.item = item;
            target.fill = FillPathOf(item);
            if (!target.fill.isEmpty()) {
                targets.append(target);
            }
        } else if (IsKnifeItem(item) && !knifeItem && CollectKnife(item, &knife, &knifeTrajectory)) {
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
        auto* curveKnife = dynamic_cast<LineAbstractItem*>(knifeItem);
        auto* shapeKnife = dynamic_cast<ShapeAbstractItem*>(knifeItem);
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
        regions = CanvasHalfRegions(scene->CanvasRect(), knife);
        if (regions.isEmpty()) {
            return false;
        }
    }

    QUndoStack* undo = scene->UndoStack();
    if (!undo) {
        return false;
    }

    // 先离线算出全部碎片。直线刀线：每个被切图元的两块画布半区交集再按
    // 互不相连拆块；曲线刀线：从填充里挖掉刀线条带后按互不相连拆块。
    // 碎片样式沿用被切图元自身的填充与描线。
    struct Fragment {
        CanvasItem* source = nullptr;
        QPainterPath piece;
    };
    QList<Fragment> fragments;
    for (const Target& target : targets) {
        // §5.4：记录原始连续块数；只有切后块数增加，或刀线确实穿过填充时才替换，
        // 避免历史多块复合图元被误判为被切开。
        const int originalBlocks = PathRegions::SplitDisjoint(target.fill).size();
        QList<QPainterPath> pieces;
        bool knifeCrosses = false;
        if (trajectoryKnife) {
            pieces = PathRegions::SplitDisjoint(target.fill.subtracted(knifeBand));
            // 曲线刀线以实际拆块为准：仅擦边相交但未切断时保留原图元。
            knifeCrosses = pieces.size() > originalBlocks;
        } else {
            bool leftHit = false;
            bool rightHit = false;
            for (int i = 0; i < regions.size(); ++i) {
                const QList<QPainterPath> parts = PathRegions::SplitDisjoint(regions.at(i).intersected(target.fill));
                pieces.append(parts);
                if (!target.fill.intersected(regions.at(i)).isEmpty()) {
                    if (i == 0) {
                        leftHit = true;
                    } else {
                        rightHit = true;
                    }
                }
            }
            // 直线刀线穿过填充：填充同时落在刀线两侧半区。
            knifeCrosses = leftHit && rightHit;
        }
        if (pieces.size() <= originalBlocks && !knifeCrosses) {
            continue;
        }

        // 同一套先吸收、无法吸收且仍极小才删除规则（§5.3、D15）。
        QList<MergeRegions::RegionBlock> blocks;
        for (const QPainterPath& piece : pieces) {
            MergeRegions::RegionBlock block;
            block.path = piece;
            block.area = PathRegions::Area(piece);
            blocks.append(block);
        }
        blocks = MergeRegions::AbsorbTinyFragments(blocks);

        // 允许掩膜：切后填充不得超出被切图元原始填充区（§5.2）。
        QPainterPath finalFill;
        for (const MergeRegions::RegionBlock& block : blocks) {
            finalFill = finalFill.isEmpty() ? block.path : finalFill.united(block.path);
        }
        if (!MergeRegions::FillWithinMask(finalFill, target.fill)) {
            continue;
        }
        if (blocks.isEmpty()) {
            continue; // 无有效结果不提交
        }
        for (const MergeRegions::RegionBlock& block : blocks) {
            fragments.append(Fragment{target.item, block.path});
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
        CanvasItem* source = fragment.source;
        // 样式 donor：被切图元是 ShapeAbstractItem 的某种形态（普通图形或多边形），
        // 两者的填充 / 描线读写已统一收口，直接按基类接口取值即可。
        auto* shapeSource = dynamic_cast<ShapeAbstractItem*>(source);
        if (!shapeSource) {
            continue;
        }
        PolygonItem::Layer layer;
        layer.path = fragment.piece;
        layer.fill = shapeSource->FillColor();
        layer.stroke = shapeSource->StrokeColor();
        layer.strokeWidth = shapeSource->StrokeWidth();
        layer.filled = true;
        layer.role = PolygonItem::LayerRole::FILLED_AREA;
        auto* item =
            new PolygonItem(QList<PolygonItem::Layer>{layer}, shapeSource->fillStyle(), shapeSource->StrokeStyle());
        item->SetName(scene->UniqueItemName(item->TypeDisplayName()));
        item->SetLayerId(source->LayerId());
        item->SetZOrder(scene->NextHeight(source->LayerId()));
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
