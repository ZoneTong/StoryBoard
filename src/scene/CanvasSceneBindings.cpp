#include "scene/CanvasScene.h"

#include "items/CanvasItem.h"
#include "items/geometry/CurveGeometry.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/text/TextItem.h"
#include "scene/Commands.h"

#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QtMath>
#include <cmath>

namespace {

qreal BindingGap(const QJsonObject& binding)
{
    const qreal gap = binding.value(QStringLiteral("gap")).toDouble(4.0);
    return gap >= 0.0 ? gap : 0.0;
}

qreal BindingFocus(const QJsonObject& binding)
{
    return binding.contains(QStringLiteral("focus")) ? binding.value(QStringLiteral("focus")).toDouble() : -1.0;
}

// 目标图元真实轮廓（场景坐标）拆成若干闭合折线；椭圆等曲线由 Qt 采样为折线。
QVector<QPolygonF> SceneOutlineRings(const CanvasItem* target)
{
    QVector<QPolygonF> rings;
    if (!target) {
        return rings;
    }
    const QPainterPath local = target->GeometryOutline();
    if (local.isEmpty()) {
        return rings;
    }
    const QPainterPath scene = target->mapToScene(local);
    const QList<QPolygonF> polygons = scene.toSubpathPolygons();
    for (const QPolygonF& polygon : polygons) {
        QPolygonF ring = polygon;
        while (ring.size() >= 2 && QLineF(ring.constFirst(), ring.constLast()).length() <= 1e-6) {
            ring.removeLast();
        }
        if (ring.size() >= 2) {
            rings.append(ring);
        }
    }
    return rings;
}

qreal OutlineLength(const QVector<QPolygonF>& rings)
{
    qreal total = 0.0;
    for (const QPolygonF& ring : rings) {
        for (int i = 0; i < ring.size(); ++i) {
            total += QLineF(ring.at(i), ring.at((i + 1) % ring.size())).length();
        }
    }
    return total;
}

// 轮廓上离 from 最近的点，附带到轮廓起点的弧长参数 focus（0..1）。
QPointF NearestOutlinePoint(const QVector<QPolygonF>& rings, const QPointF& from, qreal* focus)
{
    const qreal total = OutlineLength(rings);
    qreal walked = 0.0;
    qreal bestDistance = -1.0;
    qreal bestFocus = 0.0;
    QPointF best = from;
    for (const QPolygonF& ring : rings) {
        for (int i = 0; i < ring.size(); ++i) {
            const QPointF start = ring.at(i);
            const QPointF end = ring.at((i + 1) % ring.size());
            const QPointF edge = end - start;
            const qreal length = QLineF(start, end).length();
            if (length <= 1e-6) {
                continue;
            }
            const qreal along = qBound<qreal>(
                0.0, ((from.x() - start.x()) * edge.x() + (from.y() - start.y()) * edge.y()) / (length * length), 1.0);
            const QPointF projected = start + edge * along;
            const qreal distance = QLineF(projected, from).length();
            if (bestDistance < 0.0 || distance < bestDistance) {
                bestDistance = distance;
                best = projected;
                bestFocus = total > 0.0 ? (walked + along * length) / total : 0.0;
            }
            walked += length;
        }
    }
    if (focus) {
        *focus = qBound<qreal>(0.0, bestFocus, 1.0);
    }
    return best;
}

QPointF OutlinePointAt(const QVector<QPolygonF>& rings, qreal focus)
{
    const qreal total = OutlineLength(rings);
    if (total <= 0.0 || focus < 0.0) {
        return QPointF();
    }
    focus -= qFloor(focus);
    qreal distance = focus * total;
    for (const QPolygonF& ring : rings) {
        for (int i = 0; i < ring.size(); ++i) {
            const QPointF start = ring.at(i);
            const QPointF end = ring.at((i + 1) % ring.size());
            const qreal length = QLineF(start, end).length();
            if (length <= 1e-6) {
                continue;
            }
            if (distance <= length) {
                return start + (end - start) * (distance / length);
            }
            distance -= length;
        }
    }
    return rings.isEmpty() ? QPointF() : rings.constFirst().constFirst();
}

// 轮廓特征点：多边形顶点、相邻特征点之间的中点，以及没有明显顶点的曲线
// （椭圆 / 圆）的上下左右极点。端点落到特征点附近时校准到这些位置。
struct OutlineFeature {
    QPointF point;
    qreal focus = 0.0;
};

QPointF PointOnRing(const QPolygonF& ring, qreal distance)
{
    for (int i = 0; i < ring.size(); ++i) {
        const QPointF start = ring.at(i);
        const QPointF end = ring.at((i + 1) % ring.size());
        const qreal length = QLineF(start, end).length();
        if (length <= 1e-6) {
            continue;
        }
        if (distance <= length) {
            return start + (end - start) * (distance / length);
        }
        distance -= length;
    }
    return ring.constFirst();
}

QVector<OutlineFeature> OutlineFeatures(const QVector<QPolygonF>& rings)
{
    const qreal total = OutlineLength(rings);
    QVector<OutlineFeature> features;
    if (total <= 0.0) {
        return features;
    }
    qreal walked = 0.0;
    for (const QPolygonF& ring : rings) {
        const int count = ring.size();
        if (count < 2) {
            continue;
        }
        QVector<qreal> offsets(count + 1, 0.0);
        for (int i = 0; i < count; ++i) {
            offsets[i + 1] = offsets.at(i) + QLineF(ring.at(i), ring.at((i + 1) % count)).length();
        }
        const qreal ringLength = offsets.constLast();
        if (ringLength <= 1e-6) {
            continue;
        }
        QVector<int> corners;
        for (int i = 0; i < count; ++i) {
            if (CurveGeometry::TurnDegrees(ring.at((i + count - 1) % count), ring.at(i), ring.at((i + 1) % count)) >
                20.0) {
                corners.append(i);
            }
        }
        for (int corner : std::as_const(corners)) {
            features.append({ring.at(corner), (walked + offsets.at(corner)) / total});
        }
        if (corners.size() >= 2) {
            // 顶点之间沿轮廓取中点：矩形是四边中点，菱形是四条斜边中点。
            for (int i = 0; i < corners.size(); ++i) {
                const int first = corners.at(i);
                const int second = corners.at((i + 1) % corners.size());
                qreal start = offsets.at(first);
                qreal end = offsets.at(second);
                if (end <= start) {
                    end += ringLength;
                }
                qreal middle = (start + end) / 2.0;
                if (middle >= ringLength) {
                    middle -= ringLength;
                }
                features.append({PointOnRing(ring, middle), (walked + middle) / total});
            }
        } else {
            // 没有明显顶点的闭合曲线：把包围盒上下左右极点当作特征点。
            QVector<int> extremes;
            int minX = 0;
            int maxX = 0;
            int minY = 0;
            int maxY = 0;
            for (int i = 1; i < count; ++i) {
                if (ring.at(i).x() < ring.at(minX).x()) {
                    minX = i;
                }
                if (ring.at(i).x() > ring.at(maxX).x()) {
                    maxX = i;
                }
                if (ring.at(i).y() < ring.at(minY).y()) {
                    minY = i;
                }
                if (ring.at(i).y() > ring.at(maxY).y()) {
                    maxY = i;
                }
            }
            extremes = {minX, maxX, minY, maxY};
            for (int index : std::as_const(extremes)) {
                features.append({ring.at(index), (walked + offsets.at(index)) / total});
            }
        }
        walked += ringLength;
    }
    return features;
}

// 最近轮廓点若落在特征点附近，直接校准到特征点并同步其 focus。
constexpr qreal OUTLINE_FEATURE_SNAP = 10.0;

QPointF SnapToOutlineFeature(const QVector<QPolygonF>& rings, const QPointF& nearest, qreal* focus)
{
    if (rings.isEmpty()) {
        return nearest;
    }
    const QVector<OutlineFeature> features = OutlineFeatures(rings);
    qreal bestDistance = -1.0;
    QPointF best = nearest;
    qreal bestFocus = focus ? *focus : 0.0;
    for (const OutlineFeature& feature : features) {
        const qreal distance = QLineF(feature.point, nearest).length();
        if (distance <= OUTLINE_FEATURE_SNAP && (bestDistance < 0.0 || distance < bestDistance)) {
            bestDistance = distance;
            best = feature.point;
            bestFocus = feature.focus;
        }
    }
    if (focus && bestDistance >= 0.0) {
        *focus = bestFocus;
    }
    return best;
}

// 取离 from 最近的轮廓点；命中特征点范围时校准到特征点并返回对应 focus。
QPointF NearestOutlineAnchor(const QVector<QPolygonF>& rings, const QPointF& from, qreal* focus)
{
    qreal nearestFocus = 0.0;
    QPointF point = NearestOutlinePoint(rings, from, &nearestFocus);
    point = SnapToOutlineFeature(rings, point, &nearestFocus);
    if (focus) {
        *focus = nearestFocus;
    }
    return point;
}

// 绑定端点落在矩形哪条边：按到四条边的距离取最近的一条。
enum class EdgeSide { LEFT, RIGHT, TOP, BOTTOM };

EdgeSide SideOfBoundary(const QRectF& rect, const QPointF& point)
{
    EdgeSide side = EdgeSide::LEFT;
    qreal best = qAbs(point.x() - rect.left());
    const qreal right = qAbs(point.x() - rect.right());
    if (right < best) {
        best = right;
        side = EdgeSide::RIGHT;
    }
    const qreal top = qAbs(point.y() - rect.top());
    if (top < best) {
        best = top;
        side = EdgeSide::TOP;
    }
    const qreal bottom = qAbs(point.y() - rect.bottom());
    if (bottom < best) {
        side = EdgeSide::BOTTOM;
    }
    return side;
}

// 转折走线：首段沿一个轴走到转折位置，再沿另一个轴进入目标边；bend 是转折轴上的坐标。
// bend 取首点坐标时退化成单拐角折线（与旧行为一致），用户拖动拐点后多出一段平行段。
QVector<QPointF> OrthogonalRoute(const QRectF& endRect, const QPointF& start, const QPointF& end, qreal bend)
{
    const EdgeSide endSide = SideOfBoundary(endRect, end);
    QVector<QPointF> points;
    points.append(start);
    if (endSide == EdgeSide::LEFT || endSide == EdgeSide::RIGHT) {
        points.append(QPointF(bend, start.y()));
        points.append(QPointF(bend, end.y()));
    } else {
        points.append(QPointF(start.x(), bend));
        points.append(QPointF(end.x(), bend));
    }
    points.append(end);
    QVector<QPointF> cleaned;
    for (const QPointF& point : points) {
        if (cleaned.isEmpty() || QLineF(cleaned.constLast(), point).length() > 1e-3) {
            cleaned.append(point);
        }
    }
    return cleaned;
}

// 两端都绑定图形时的走线点：直连两点；折线按末端边的进入方向排成横纵轴折线。
QVector<QPointF> BoundLineRoute(CanvasScene* scene, StraightLineItem* line, StraightLineItem::RouteMode route)
{
    if (!scene || !line) {
        return {};
    }
    const QJsonObject startBinding = line->StartBinding();
    const QJsonObject endBinding = line->EndBinding();
    if (startBinding.isEmpty() || endBinding.isEmpty()) {
        return {};
    }
    CanvasItem* startTarget = scene->ItemByUid(startBinding.value(QStringLiteral("elementId")).toString());
    CanvasItem* endTarget = scene->ItemByUid(endBinding.value(QStringLiteral("elementId")).toString());
    if (!startTarget || !endTarget || startTarget->Category() != ItemCategory::SHAPE ||
        endTarget->Category() != ItemCategory::SHAPE) {
        return {};
    }
    const QVector<QPointF> knots = line->StrokePoints();
    if (knots.size() < 2) {
        return {};
    }
    const QPointF start = CanvasScene::AttachmentPoint(startTarget, line->mapToScene(knots.at(1)),
                                                       BindingGap(startBinding), BindingFocus(startBinding));
    const QPointF end = CanvasScene::AttachmentPoint(endTarget, line->mapToScene(knots.constLast()),
                                                     BindingGap(endBinding), BindingFocus(endBinding));
    if (route == StraightLineItem::RouteMode::ORTHOGONAL) {
        const QRectF endRect = endTarget->mapRectToScene(endTarget->ContentRect());
        const EdgeSide endSide = SideOfBoundary(endRect, end);
        // 沿用当前转折位置：图形移动后转折不会跳回默认；首次重排仍从单拐角开始。
        const bool horizontalEntry = endSide == EdgeSide::LEFT || endSide == EdgeSide::RIGHT;
        qreal bend = horizontalEntry ? start.x() : start.y();
        if (knots.size() >= 3) {
            const QPointF corner = line->mapToScene(knots.at(1));
            bend = horizontalEntry ? corner.x() : corner.y();
        }
        return OrthogonalRoute(endRect, start, end, bend);
    }
    return QVector<QPointF>{start, end};
}

} // namespace

CanvasItem* CanvasScene::ItemByUid(const QString& uid) const
{
    if (uid.isEmpty()) {
        return nullptr;
    }
    for (CanvasItem* item : CanvasItems()) {
        if (item && item->Uid() == uid) {
            return item;
        }
    }
    return nullptr;
}

void CanvasScene::SetBindingHighlight(CanvasItem* item)
{
    if (m_bindingHighlight == item) {
        return;
    }
    m_bindingHighlight = item;
    emit bindingHighlightChanged();
}

QPointF CanvasScene::AttachmentPoint(const CanvasItem* target, const QPointF& from, qreal gap, qreal focus)
{
    if (!target) {
        return from;
    }
    const QRectF rect = target->mapRectToScene(target->ContentRect());
    const QVector<QPolygonF> rings = SceneOutlineRings(target);
    if (rings.isEmpty()) {
        return from;
    }
    QPointF point;
    if (focus >= 0.0) {
        point = OutlinePointAt(rings, focus);
    } else {
        point = NearestOutlineAnchor(rings, from, nullptr);
    }
    QPointF outward = point - rect.center();
    const qreal length = qSqrt(outward.x() * outward.x() + outward.y() * outward.y());
    if (length > 0.0 && gap > 0.0) {
        point += outward / length * gap;
    }
    return point;
}

qreal CanvasScene::OutlineFocusForPoint(const CanvasItem* target, const QPointF& point)
{
    if (!target) {
        return 0.0;
    }
    const QVector<QPolygonF> rings = SceneOutlineRings(target);
    if (rings.isEmpty()) {
        return 0.0;
    }
    qreal focus = 0.0;
    NearestOutlineAnchor(rings, point, &focus);
    return focus;
}

CanvasItem* CanvasScene::BindableShapeAt(const QPointF& scenePos, qreal maxDistance) const
{
    const QList<CanvasItem*> items = CanvasItems();
    // 叠放靠后的在上层，吸附优先命中最上面那个。
    for (int i = items.size() - 1; i >= 0; --i) {
        CanvasItem* item = items.at(i);
        if (!item || item->Category() != ItemCategory::SHAPE || item->Uid().isEmpty() || !item->isVisible()) {
            continue;
        }
        const QRectF rect = item->mapRectToScene(item->ContentRect());
        if (rect.contains(scenePos)) {
            return item;
        }
        const qreal dx = qMax<qreal>(qMax(rect.left() - scenePos.x(), 0.0), scenePos.x() - rect.right());
        const qreal dy = qMax<qreal>(qMax(rect.top() - scenePos.y(), 0.0), scenePos.y() - rect.bottom());
        if (dx * dx + dy * dy <= maxDistance * maxDistance) {
            return item;
        }
    }
    return nullptr;
}

void CanvasScene::SyncArrowBindings()
{
    const QList<CanvasItem*> items = CanvasItems();
    // 每次交互提交只建一次 uid 索引，避免逐条绑定再全表扫一遍。
    QHash<QString, CanvasItem*> byUid;
    byUid.reserve(items.size());
    for (CanvasItem* item : items) {
        if (item && !item->Uid().isEmpty()) {
            byUid.insert(item->Uid(), item);
        }
    }
    for (CanvasItem* canvasItem : items) {
        LineAbstractItem* line = dynamic_cast<LineAbstractItem*>(canvasItem);
        if (!line) {
            continue;
        }
        const QVector<QPointF> knots = line->StrokePoints();
        if (knots.size() < 2) {
            continue;
        }
        // 两端都绑定的直线箭头若选了折线，按两个连接点整体重排折线，不再逐端挪点。
        if (auto* straight = dynamic_cast<StraightLineItem*>(line);
            straight && straight->LineRoute() == StraightLineItem::RouteMode::ORTHOGONAL) {
            const QVector<QPointF> routed = BoundLineRoute(this, straight, straight->LineRoute());
            if (!routed.isEmpty()) {
                straight->ApplyRouteGeometry(routed);
                continue;
            }
        }
        if (!line->StartBinding().isEmpty()) {
            const QJsonObject binding = line->StartBinding();
            CanvasItem* target = byUid.value(binding.value(QStringLiteral("elementId")).toString(), nullptr);
            if (target && target->Category() == ItemCategory::SHAPE) {
                const QPointF other = line->mapToScene(knots.at(1));
                line->MoveEndpoint(true, AttachmentPoint(target, other, BindingGap(binding), BindingFocus(binding)));
            }
        }
        if (!line->EndBinding().isEmpty()) {
            const QJsonObject binding = line->EndBinding();
            CanvasItem* target = byUid.value(binding.value(QStringLiteral("elementId")).toString(), nullptr);
            if (target && target->Category() == ItemCategory::SHAPE) {
                // 起点可能刚被移动过，终点参考点要用最新的节点。
                const QVector<QPointF> current = line->StrokePoints();
                const QPointF other = line->mapToScene(current.at(current.size() - 2));
                line->MoveEndpoint(false, AttachmentPoint(target, other, BindingGap(binding), BindingFocus(binding)));
            }
        }
    }
}

bool CanvasScene::SetStraightLineRoute(StraightLineItem* line, int route, const QString& text)
{
    if (!line) {
        return false;
    }
    const StraightLineItem::RouteMode mode = static_cast<StraightLineItem::RouteMode>(route);
    if (line->LineRoute() == mode) {
        return false;
    }
    const QVector<QPointF> points = BoundLineRoute(this, line, mode);
    if (points.isEmpty()) {
        // 只有两端都绑在图形上才有“走线形态”可调。
        return false;
    }
    const int beforeRoute = static_cast<int>(line->LineRoute());
    const LineAbstractItem::GeometryState before = line->geometryState();
    line->SetLineRoute(mode);
    line->ApplyRouteGeometry(points);
    const LineAbstractItem::GeometryState after = line->geometryState();
    if (m_undoStack) {
        m_undoStack->push(new LineRouteCommand(this, line, beforeRoute, before, route, after, text));
    }
    update();
    return true;
}

void CanvasScene::SyncContainerText(TextItem* text)
{
    if (!text) {
        return;
    }
    const QString uid = text->ContainerUid();
    if (uid.isEmpty()) {
        return;
    }
    CanvasItem* container = ItemByUid(uid);
    if (!container || container->Category() != ItemCategory::SHAPE) {
        return;
    }
    const QRectF rect = container->mapRectToScene(container->ContentRect());
    const QSizeF size = text->ContainerLayoutSize(rect.width());
    text->SetContentSize(size);
    text->setPos(QPointF(rect.left(), rect.center().y() - size.height() / 2.0));
}

void CanvasScene::SyncContainerTexts()
{
    // 正在被拖动的容器文字先不动：用户可能正把它拖出图形来解绑，松手后再由交互层定归属。
    const QGraphicsItem* grabbed = mouseGrabberItem();
    for (CanvasItem* canvasItem : CanvasItems()) {
        if (auto* text = dynamic_cast<TextItem*>(canvasItem)) {
            if (text != grabbed) {
                SyncContainerText(text);
            }
        }
    }
}

CanvasItem* CanvasScene::ContainerShapeForText(const TextItem* text) const
{
    if (!text) {
        return nullptr;
    }
    const QPointF center = text->mapToScene(text->ContentRect().center());
    const QList<CanvasItem*> items = CanvasItems();
    // 叠放靠后的在上层：文字中心同时落在多个图形里时取最上面那个。
    for (int i = items.size() - 1; i >= 0; --i) {
        CanvasItem* item = items.at(i);
        if (!item || item == text || item->Category() != ItemCategory::SHAPE) {
            continue;
        }
        if (item->Uid().isEmpty() || !item->isVisible() || !IsItemEditable(item)) {
            continue;
        }
        if (item->mapRectToScene(item->ContentRect()).contains(center)) {
            return item;
        }
    }
    return nullptr;
}

bool CanvasScene::SetTextContainer(TextItem* text, const QString& containerUid, const QString& label)
{
    if (!text) {
        return false;
    }
    const TextItemState before = text->State();
    return SetTextContainer(text, containerUid, text->ContainerUid(), before, label);
}

bool CanvasScene::SetTextContainer(TextItem* text, const QString& containerUid, const QString& beforeUid,
                                   const TextItemState& beforeState, const QString& label)
{
    if (!text) {
        return false;
    }
    const QString afterUid = containerUid.trimmed();
    if (afterUid == text->ContainerUid()) {
        return false;
    }
    // 绑定：按目标容器算折行与居中；解绑：保留文字自己在画布上的位置与尺寸。
    TextItemState after = text->State();
    if (!afterUid.isEmpty()) {
        CanvasItem* container = ItemByUid(afterUid);
        if (container && container->Category() == ItemCategory::SHAPE) {
            const QRectF rect = container->mapRectToScene(container->ContentRect());
            after.size = text->ContainerLayoutSize(rect.width());
            after.pos = QPointF(rect.left(), rect.center().y() - after.size.height() / 2.0);
        }
    }
    if (m_undoStack) {
        m_undoStack->push(new TextContainerCommand(this, text, beforeUid, beforeState, afterUid, after, label));
    } else {
        text->SetContainerUid(afterUid);
        text->ApplyState(after);
        SyncContainerText(text);
    }
    update();
    return true;
}
