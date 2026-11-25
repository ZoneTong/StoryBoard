#include "scene/CanvasScene.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/text/TextItem.h"

#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QtMath>

namespace {

// 从 from 朝 target 的矩形中心射出，取矩形边界上的落点；端点落在图形内部时也按最近边贴出。
QPointF BoundaryPoint(const QRectF& rect, const QPointF& from, qreal gap)
{
    if (rect.isEmpty()) {
        return from;
    }
    QPointF direction = rect.center() - from;
    if (qFuzzyIsNull(direction.x()) && qFuzzyIsNull(direction.y())) {
        direction = QPointF(1.0, 0.0);
    }
    qreal best = -1.0;
    auto consider = [&best, &direction](qreal delta, qreal span) {
        if (!qFuzzyIsNull(delta)) {
            const qreal ratio = span / delta;
            if (ratio > 0.0 && (best < 0.0 || ratio < best)) {
                best = ratio;
            }
        }
    };
    consider(direction.x(), rect.right() - from.x());
    consider(direction.x(), rect.left() - from.x());
    consider(direction.y(), rect.bottom() - from.y());
    consider(direction.y(), rect.top() - from.y());
    if (best < 0.0) {
        return rect.center();
    }
    QPointF point = from + direction * best;
    // gap 是端点与图形边缘的留白，沿“中心 → 落点”方向再往外推。
    QPointF outward = point - rect.center();
    const qreal length = qSqrt(outward.x() * outward.x() + outward.y() * outward.y());
    if (length > 0.0 && gap > 0.0) {
        point += outward / length * gap;
    }
    return point;
}

qreal BindingGap(const QJsonObject& binding)
{
    const qreal gap = binding.value(QStringLiteral("gap")).toDouble(4.0);
    return gap >= 0.0 ? gap : 0.0;
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
    QPointF point = focus >= 0.0 ? PointForFocus(rect, focus) : BoundaryPoint(rect, from, 0.0);
    QPointF outward = point - rect.center();
    const qreal length = qSqrt(outward.x() * outward.x() + outward.y() * outward.y());
    if (length > 0.0 && gap > 0.0) {
        point += outward / length * gap;
    }
    return point;
}

qreal CanvasScene::FocusForPoint(const QRectF& rect, const QPointF& point)
{
    if (rect.width() <= 0.0 || rect.height() <= 0.0) {
        return 0.0;
    }
    const qreal total = 2.0 * (rect.width() + rect.height());
    qreal bestDistance = -1.0;
    qreal focus = 0.0;
    auto consider = [&](const QPointF& start, const QPointF& end, qreal base) {
        const QPointF edge = end - start;
        const qreal length = QLineF(start, end).length();
        if (length <= 0.0) {
            return;
        }
        const qreal along =
            qBound<qreal>(0.0, ((point.x() - start.x()) * edge.x() + (point.y() - start.y()) * edge.y()) /
                                   (length * length),
                          1.0);
        const QPointF projected = start + edge * along;
        const qreal distance = QLineF(projected, point).length();
        if (bestDistance < 0.0 || distance < bestDistance) {
            bestDistance = distance;
            focus = (base + along * length) / total;
        }
    };
    const QPointF topLeft = rect.topLeft();
    const QPointF topRight = rect.topRight();
    const QPointF bottomRight = rect.bottomRight();
    const QPointF bottomLeft = rect.bottomLeft();
    consider(topLeft, topRight, 0.0);
    consider(topRight, bottomRight, rect.width());
    consider(bottomRight, bottomLeft, rect.width() + rect.height());
    consider(bottomLeft, topLeft, 2.0 * rect.width() + rect.height());
    return qBound<qreal>(0.0, focus, 1.0);
}

QPointF CanvasScene::PointForFocus(const QRectF& rect, qreal focus)
{
    if (rect.width() <= 0.0 || rect.height() <= 0.0) {
        return rect.center();
    }
    if (focus < 0.0) {
        focus = 0.0;
    }
    focus -= qFloor(focus);
    const qreal w = rect.width();
    const qreal h = rect.height();
    qreal distance = focus * 2.0 * (w + h);
    if (distance <= w) {
        return QPointF(rect.left() + distance, rect.top());
    }
    distance -= w;
    if (distance <= h) {
        return QPointF(rect.right(), rect.top() + distance);
    }
    distance -= h;
    if (distance <= w) {
        return QPointF(rect.right() - distance, rect.bottom());
    }
    distance -= w;
    return QPointF(rect.left(), rect.bottom() - qMin(distance, h));
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
        if (!line->StartBinding().isEmpty()) {
            const QJsonObject binding = line->StartBinding();
            CanvasItem* target = byUid.value(binding.value(QStringLiteral("elementId")).toString(), nullptr);
            if (target && target->Category() == ItemCategory::SHAPE) {
                const QPointF other = line->mapToScene(knots.at(1));
                const qreal focus =
                    binding.contains(QStringLiteral("focus")) ? binding.value(QStringLiteral("focus")).toDouble() : -1.0;
                line->MoveEndpoint(true, AttachmentPoint(target, other, BindingGap(binding), focus));
            }
        }
        if (!line->EndBinding().isEmpty()) {
            const QJsonObject binding = line->EndBinding();
            CanvasItem* target = byUid.value(binding.value(QStringLiteral("elementId")).toString(), nullptr);
            if (target && target->Category() == ItemCategory::SHAPE) {
                // 起点可能刚被移动过，终点参考点要用最新的节点。
                const QVector<QPointF> current = line->StrokePoints();
                const QPointF other = line->mapToScene(current.at(current.size() - 2));
                const qreal focus =
                    binding.contains(QStringLiteral("focus")) ? binding.value(QStringLiteral("focus")).toDouble() : -1.0;
                line->MoveEndpoint(false, AttachmentPoint(target, other, BindingGap(binding), focus));
            }
        }
    }
}

void CanvasScene::SyncContainerTexts()
{
    const QList<CanvasItem*> items = CanvasItems();
    QHash<QString, CanvasItem*> byUid;
    byUid.reserve(items.size());
    for (CanvasItem* item : items) {
        if (item && !item->Uid().isEmpty()) {
            byUid.insert(item->Uid(), item);
        }
    }
    for (CanvasItem* canvasItem : items) {
        TextItem* text = dynamic_cast<TextItem*>(canvasItem);
        if (!text || text->ContainerUid().isEmpty()) {
            continue;
        }
        CanvasItem* container = byUid.value(text->ContainerUid(), nullptr);
        if (!container || container->Category() != ItemCategory::SHAPE) {
            continue;
        }
        const QRectF rect = container->mapRectToScene(container->ContentRect());
        const QSizeF size = text->ContainerLayoutSize(rect.width());
        text->SetContentSize(size);
        text->setPos(QPointF(rect.left(), rect.center().y() - size.height() / 2.0));
    }
}

