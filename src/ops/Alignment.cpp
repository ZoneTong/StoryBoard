#include "ops/Alignment.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"

#include <QRectF>
#include <algorithm>

namespace {

QList<CanvasItem*> AlignmentTargets(CanvasScene* scene)
{
    QList<CanvasItem*> items;
    if (!scene) {
        return items;
    }
    for (CanvasItem* item : scene->SelectedCanvasItems()) {
        if (item && scene->IsItemEditable(item)) {
            items.append(item);
        }
    }
    return items;
}

QRectF UnionRect(const QList<CanvasItem*>& items)
{
    QRectF bounds;
    for (CanvasItem* item : items) {
        bounds = bounds.isNull() ? item->SceneContentRect() : bounds.united(item->SceneContentRect());
    }
    return bounds;
}

// 等间距分布：先按轴排序，再按“可用空隙平均分配”计算每个图元的目标位置。
QList<qreal> DistributeDeltas(QList<CanvasItem*> items, bool horizontal, const QRectF& bounds)
{
    std::sort(items.begin(), items.end(), [horizontal](CanvasItem* left, CanvasItem* right) {
        const QRectF a = left->SceneContentRect();
        const QRectF b = right->SceneContentRect();
        return horizontal ? a.left() < b.left() : a.top() < b.top();
    });

    const qreal span = horizontal ? bounds.width() : bounds.height();
    qreal used = 0.0;
    for (CanvasItem* item : items) {
        const QRectF rect = item->SceneContentRect();
        used += horizontal ? rect.width() : rect.height();
    }
    const int gaps = qMax(1, items.size() - 1);
    const qreal step = (span - used) / gaps;

    QList<qreal> deltas;
    qreal cursor = horizontal ? bounds.left() : bounds.top();
    for (CanvasItem* item : items) {
        const QRectF rect = item->SceneContentRect();
        deltas.append(cursor - (horizontal ? rect.left() : rect.top()));
        cursor += (horizontal ? rect.width() : rect.height()) + step;
    }
    return deltas;
}

} // namespace

namespace Alignment {

bool Supports(CanvasScene* scene, Mode mode)
{
    const int count = AlignmentTargets(scene).size();
    if (mode == Mode::DISTRIBUTE_HORIZONTAL || mode == Mode::DISTRIBUTE_VERTICAL) {
        return count >= 3;
    }
    return count >= 2;
}

int Apply(CanvasScene* scene, Mode mode, const QString& commandText)
{
    QList<CanvasItem*> items = AlignmentTargets(scene);
    if (items.size() < 2) {
        return 0;
    }
    const QRectF bounds = UnionRect(items);
    if (bounds.isNull()) {
        return 0;
    }

    QList<qreal> dxs;
    QList<qreal> dys;
    const bool distribute = mode == Mode::DISTRIBUTE_HORIZONTAL || mode == Mode::DISTRIBUTE_VERTICAL;
    if (distribute) {
        if (items.size() < 3) {
            return 0;
        }
        const bool horizontal = mode == Mode::DISTRIBUTE_HORIZONTAL;
        items = [&items, horizontal]() {
            QList<CanvasItem*> sorted = items;
            std::sort(sorted.begin(), sorted.end(), [horizontal](CanvasItem* left, CanvasItem* right) {
                const QRectF a = left->SceneContentRect();
                const QRectF b = right->SceneContentRect();
                return horizontal ? a.left() < b.left() : a.top() < b.top();
            });
            return sorted;
        }();
        const QList<qreal> deltas = DistributeDeltas(items, horizontal, bounds);
        for (int i = 0; i < items.size(); ++i) {
            dxs.append(horizontal ? deltas.at(i) : 0.0);
            dys.append(horizontal ? 0.0 : deltas.at(i));
        }
    } else {
        for (CanvasItem* item : items) {
            const QRectF rect = item->SceneContentRect();
            qreal dx = 0.0;
            qreal dy = 0.0;
            switch (mode) {
            case Mode::LEFT:
                dx = bounds.left() - rect.left();
                break;
            case Mode::HORIZONTAL_CENTER:
                dx = bounds.center().x() - rect.center().x();
                break;
            case Mode::RIGHT:
                dx = bounds.right() - rect.right();
                break;
            case Mode::TOP:
                dy = bounds.top() - rect.top();
                break;
            case Mode::VERTICAL_CENTER:
                dy = bounds.center().y() - rect.center().y();
                break;
            case Mode::BOTTOM:
                dy = bounds.bottom() - rect.bottom();
                break;
            case Mode::DISTRIBUTE_HORIZONTAL:
            case Mode::DISTRIBUTE_VERTICAL:
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
        before.append({item, item->pos(), item->ContentSize()});
        after.append({item, item->pos() + delta, item->ContentSize()});
        if (!qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y())) {
            ++moved;
        }
    }
    if (moved > 0) {
        scene->PushGeometry(before, after, commandText);
    }
    return moved;
}

} // namespace Alignment
