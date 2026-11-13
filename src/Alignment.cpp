#include "Alignment.h"

#include "CanvasItem.h"
#include "CanvasScene.h"

#include <QRectF>
#include <algorithm>

namespace {

QList<CanvasItem*> alignmentTargets(CanvasScene* scene)
{
    QList<CanvasItem*> items;
    if (!scene) {
        return items;
    }
    for (CanvasItem* item : scene->selectedCanvasItems()) {
        if (item && scene->isItemEditable(item)) {
            items.append(item);
        }
    }
    return items;
}

QRectF unionRect(const QList<CanvasItem*>& items)
{
    QRectF bounds;
    for (CanvasItem* item : items) {
        bounds = bounds.isNull() ? item->sceneContentRect() : bounds.united(item->sceneContentRect());
    }
    return bounds;
}

// 等间距分布：先按轴排序，再按“可用空隙平均分配”计算每个图元的目标位置。
QList<qreal> distributeDeltas(QList<CanvasItem*> items, bool horizontal, const QRectF& bounds)
{
    std::sort(items.begin(), items.end(), [horizontal](CanvasItem* left, CanvasItem* right) {
        const QRectF a = left->sceneContentRect();
        const QRectF b = right->sceneContentRect();
        return horizontal ? a.left() < b.left() : a.top() < b.top();
    });

    const qreal span = horizontal ? bounds.width() : bounds.height();
    qreal used = 0.0;
    for (CanvasItem* item : items) {
        const QRectF rect = item->sceneContentRect();
        used += horizontal ? rect.width() : rect.height();
    }
    const int gaps = qMax(1, items.size() - 1);
    const qreal step = (span - used) / gaps;

    QList<qreal> deltas;
    qreal cursor = horizontal ? bounds.left() : bounds.top();
    for (CanvasItem* item : items) {
        const QRectF rect = item->sceneContentRect();
        deltas.append(cursor - (horizontal ? rect.left() : rect.top()));
        cursor += (horizontal ? rect.width() : rect.height()) + step;
    }
    return deltas;
}

} // namespace

namespace Alignment {

bool supports(CanvasScene* scene, Mode mode)
{
    const int count = alignmentTargets(scene).size();
    if (mode == Mode::DistributeHorizontal || mode == Mode::DistributeVertical) {
        return count >= 3;
    }
    return count >= 2;
}

int apply(CanvasScene* scene, Mode mode, const QString& commandText)
{
    QList<CanvasItem*> items = alignmentTargets(scene);
    if (items.size() < 2) {
        return 0;
    }
    const QRectF bounds = unionRect(items);
    if (bounds.isNull()) {
        return 0;
    }

    QList<qreal> dxs;
    QList<qreal> dys;
    const bool distribute =
        mode == Mode::DistributeHorizontal || mode == Mode::DistributeVertical;
    if (distribute) {
        if (items.size() < 3) {
            return 0;
        }
        const bool horizontal = mode == Mode::DistributeHorizontal;
        items = [&items, horizontal]() {
            QList<CanvasItem*> sorted = items;
            std::sort(sorted.begin(), sorted.end(), [horizontal](CanvasItem* left, CanvasItem* right) {
                const QRectF a = left->sceneContentRect();
                const QRectF b = right->sceneContentRect();
                return horizontal ? a.left() < b.left() : a.top() < b.top();
            });
            return sorted;
        }();
        const QList<qreal> deltas = distributeDeltas(items, horizontal, bounds);
        for (int i = 0; i < items.size(); ++i) {
            dxs.append(horizontal ? deltas.at(i) : 0.0);
            dys.append(horizontal ? 0.0 : deltas.at(i));
        }
    } else {
        for (CanvasItem* item : items) {
            const QRectF rect = item->sceneContentRect();
            qreal dx = 0.0;
            qreal dy = 0.0;
            switch (mode) {
            case Mode::Left:
                dx = bounds.left() - rect.left();
                break;
            case Mode::HorizontalCenter:
                dx = bounds.center().x() - rect.center().x();
                break;
            case Mode::Right:
                dx = bounds.right() - rect.right();
                break;
            case Mode::Top:
                dy = bounds.top() - rect.top();
                break;
            case Mode::VerticalCenter:
                dy = bounds.center().y() - rect.center().y();
                break;
            case Mode::Bottom:
                dy = bounds.bottom() - rect.bottom();
                break;
            case Mode::DistributeHorizontal:
            case Mode::DistributeVertical:
                break;
            }
            dxs.append(dx);
            dys.append(dy);
        }
    }

    ItemGeometryList before;
    ItemGeometryList after;
    int moved = 0;
    for (int i = 0; i < items.size(); ++i) {
        CanvasItem* item = items.at(i);
        const QPointF delta(dxs.at(i), dys.at(i));
        before.append({item, item->pos(), item->contentSize()});
        after.append({item, item->pos() + delta, item->contentSize()});
        if (!qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y())) {
            ++moved;
        }
    }
    if (moved > 0) {
        scene->pushGeometry(before, after, commandText);
    }
    return moved;
}

} // namespace Alignment
