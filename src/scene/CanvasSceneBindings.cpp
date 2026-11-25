#include "scene/CanvasScene.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/text/TextItem.h"

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

QPointF CanvasScene::AttachmentPoint(const CanvasItem* target, const QPointF& from, qreal gap)
{
    if (!target) {
        return from;
    }
    return BoundaryPoint(target->mapRectToScene(target->ContentRect()), from, gap);
}

void CanvasScene::SyncArrowBindings()
{
    for (CanvasItem* canvasItem : CanvasItems()) {
        LineAbstractItem* line = dynamic_cast<LineAbstractItem*>(canvasItem);
        if (!line) {
            continue;
        }
        const QVector<QPointF> knots = line->StrokePoints();
        if (knots.size() < 2) {
            continue;
        }
        if (!line->StartBinding().isEmpty()) {
            CanvasItem* target = ItemByUid(line->StartBinding().value(QStringLiteral("elementId")).toString());
            if (target && target->Category() == ItemCategory::SHAPE) {
                const QPointF other = line->mapToScene(knots.at(1));
                const QRectF rect = target->mapRectToScene(target->ContentRect());
                line->MoveEndpoint(true, BoundaryPoint(rect, other, BindingGap(line->StartBinding())));
            }
        }
        if (!line->EndBinding().isEmpty()) {
            CanvasItem* target = ItemByUid(line->EndBinding().value(QStringLiteral("elementId")).toString());
            if (target && target->Category() == ItemCategory::SHAPE) {
                // 起点可能刚被移动过，终点参考点要用最新的节点。
                const QVector<QPointF> current = line->StrokePoints();
                const QPointF other = line->mapToScene(current.at(current.size() - 2));
                const QRectF rect = target->mapRectToScene(target->ContentRect());
                line->MoveEndpoint(false, BoundaryPoint(rect, other, BindingGap(line->EndBinding())));
            }
        }
    }
}

void CanvasScene::SyncContainerTexts()
{
    for (CanvasItem* canvasItem : CanvasItems()) {
        TextItem* text = dynamic_cast<TextItem*>(canvasItem);
        if (!text || text->ContainerUid().isEmpty()) {
            continue;
        }
        CanvasItem* container = ItemByUid(text->ContainerUid());
        if (!container || container->Category() != ItemCategory::SHAPE) {
            continue;
        }
        const QRectF rect = container->mapRectToScene(container->ContentRect());
        const QSizeF size = text->NaturalSize();
        text->setPos(rect.center() - QPointF(size.width() / 2.0, size.height() / 2.0));
    }
}

