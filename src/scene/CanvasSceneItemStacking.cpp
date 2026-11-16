#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "scene/ItemHeightPlanner.h"
#include "scene/SceneConstants.h"

#include <QSet>
#include <QUndoStack>
#include <algorithm>
#include <utility>

qreal CanvasScene::layerBaseZ(int layerId) const
{
    const int index = layerIndex(layerId);
    return index > 0 ? index * scene_constants::kLayerZStride : 0.0;
}

void CanvasScene::applyItemStacking(CanvasItem* item)
{
    if (!item) {
        return;
    }
    const int maxHeight = scene_constants::kMaxItemHeight;
    const int height = qBound(0, item->height(), maxHeight);
    if (height != item->height()) {
        item->setHeight(height);
    }
    item->setZValue(layerBaseZ(item->layerId()) + height);
}

void CanvasScene::applyAllItemStacking()
{
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        applyItemStacking(item);
    }
}

int CanvasScene::nextHeight(int layerId) const
{
    return ItemHeightPlanner().nextHeight(canvasItems(), layerId);
}

bool CanvasScene::layerHeightsUnique(int layerId) const
{
    return ItemHeightPlanner().heightsUnique(layerItems(layerId));
}

bool CanvasScene::heightUsedByOther(int layerId, int height, const CanvasItem* except) const
{
    return ItemHeightPlanner().heightUsedByOther(layerItems(layerId), height, except);
}

bool CanvasScene::normalizeLayerHeights(int layerId)
{
    QList<CanvasItem*> items = layerItems(layerId);
    if (items.isEmpty()) {
        return false;
    }

    QSet<int> seen;
    bool needsNormalization = false;
    for (const CanvasItem* item : std::as_const(items)) {
        if (item->height() < 0 || item->height() > scene_constants::kMaxItemHeight || seen.contains(item->height())) {
            needsNormalization = true;
            break;
        }
        seen.insert(item->height());
    }
    if (!needsNormalization) {
        return false;
    }

    ItemStackList before;
    before.reserve(items.size());
    for (const CanvasItem* item : std::as_const(items)) {
        before.append({item, item->layerId(), item->height()});
    }

    std::stable_sort(items.begin(), items.end(), [](const CanvasItem* a, const CanvasItem* b) {
        if (a->height() != b->height()) {
            return a->height() < b->height();
        }
        return a->zValue() < b->zValue();
    });
    for (int i = 0; i < items.size(); ++i) {
        CanvasItem* item = items.at(i);
        item->setHeight(i + 1);
        applyItemStacking(item);
        item->update();
    }

    // 归一化是真实的状态修复，必须能被撤销；fromJson 加载期间的修复不算
    // 用户操作（m_restoringDocument 守卫），不往刚清空的撤销栈里压命令。
    if (m_undoStack && !m_restoringDocument) {
        ItemStackList after;
        after.reserve(items.size());
        for (CanvasItem* item : std::as_const(items)) {
            after.append({item, item->layerId(), item->height()});
        }
        m_undoStack->push(new ItemStackCommand(this, before, after, tr("Repair item stacking")));
    }
    return true;
}

void CanvasScene::normalizeAllHeights()
{
    for (const CanvasLayer& entry : std::as_const(m_layers)) {
        if (entry.id != BackgroundLayerId) {
            normalizeLayerHeights(entry.id);
        }
    }
}

ItemStackList CanvasScene::itemStackSnapshot(const QList<CanvasItem*>& items) const
{
    ItemStackList snapshot;
    for (CanvasItem* item : items) {
        if (item) {
            snapshot.append({item, item->layerId(), item->height()});
        }
    }
    return snapshot;
}

bool CanvasScene::adjustItemHeight(CanvasItem* item, int delta, const QString& text)
{
    if (!item || delta == 0 || !hasLayer(item->layerId())) {
        return false;
    }

    const int current = item->height();
    const int target = qBound(0, current + delta, scene_constants::kMaxItemHeight);
    if (target == current) {
        return false;
    }

    CanvasItem* colliding = nullptr;
    const QList<CanvasItem*> peers = layerItems(item->layerId());
    for (CanvasItem* peer : peers) {
        if (peer != item && peer->height() == target) {
            colliding = peer;
            break;
        }
    }

    const ItemStackList before =
        itemStackSnapshot(colliding ? QList<CanvasItem*>{item, colliding} : QList<CanvasItem*>{item});
    ItemStackList after;
    after.append({item, item->layerId(), target});
    if (colliding) {
        after.append({colliding, colliding->layerId(), current});
    }

    const QString commandText = text.isEmpty() ? (delta > 0 ? tr("Move item up") : tr("Move item down")) : text;
    if (m_undoStack) {
        m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
    } else {
        applyItemStack(after);
    }
    return true;
}

int CanvasScene::adjustSelectedHeights(int delta)
{
    if (delta == 0) {
        return 0;
    }

    QList<CanvasItem*> targets;
    const QList<CanvasItem*> selected = selectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (item && isItemEditable(item)) {
            targets.append(item);
        }
    }
    if (targets.isEmpty()) {
        return 0;
    }

    // 多选整块移动「全或不动」：任一目标越出 0–kMaxItemHeight 边界，或目标
    // 的新高度撞上未选中的图元，就整体不动。否则逐个 swap 会出现「底部两项
    // 按下移，被互相换位顶到最上面」的错乱结果。
    if (targets.size() > 1) {
        QHash<int, QSet<CanvasItem*>> targetsByLayer;
        QHash<int, QSet<int>> newHeightsByLayer;
        for (CanvasItem* item : targets) {
            targetsByLayer[item->layerId()].insert(item);
            newHeightsByLayer[item->layerId()].insert(item->height() + delta);
        }
        bool blocked = false;
        for (auto it = targetsByLayer.constBegin(); it != targetsByLayer.constEnd(); ++it) {
            const QSet<int>& newHeights = newHeightsByLayer[it.key()];
            for (CanvasItem* peer : layerItems(it.key())) {
                if (!peer) {
                    continue;
                }
                if (it.value().contains(peer)) {
                    const int targetHeight = peer->height() + delta;
                    if (targetHeight < 0 || targetHeight > scene_constants::kMaxItemHeight) {
                        blocked = true;
                        break;
                    }
                } else if (newHeights.contains(peer->height())) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) {
                break;
            }
        }
        if (blocked) {
            return 0;
        }
        const ItemStackList before = itemStackSnapshot(targets);
        ItemStackList after;
        after.reserve(targets.size());
        for (CanvasItem* item : targets) {
            after.append({item, item->layerId(), item->height() + delta});
        }
        const QString commandText = delta > 0 ? tr("Move items up") : tr("Move items down");
        if (m_undoStack) {
            m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
        } else {
            applyItemStack(after);
        }
        return targets.size();
    }

    // 单选保持原有「与相邻图元互换」语义。
    return adjustItemHeight(targets.first(), delta) ? 1 : 0;
}

int CanvasScene::moveSelectedItemsToAdjacentLayer(int delta)
{
    if (delta == 0) {
        return 0;
    }

    QList<CanvasItem*> targets;
    const QList<CanvasItem*> selected = selectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (item && isItemEditable(item)) {
            targets.append(item);
        }
    }
    if (targets.isEmpty()) {
        return 0;
    }

    const bool grouped = m_undoStack && targets.size() > 1;
    if (grouped) {
        m_undoStack->beginMacro(delta > 0 ? tr("Move to the layer above") : tr("Move to the layer below"));
    }

    int changed = 0;
    for (CanvasItem* item : targets) {
        if (moveItemToAdjacentLayer(item, delta)) {
            ++changed;
        }
    }

    if (grouped) {
        m_undoStack->endMacro();
    }
    return changed;
}

bool CanvasScene::moveItemToAdjacentLayer(CanvasItem* item, int delta, const QString& text)
{
    if (!item || delta == 0) {
        return false;
    }

    const int currentIndex = layerIndex(item->layerId());
    const int targetIndex = currentIndex + delta;
    if (currentIndex <= 0 || targetIndex <= 0 || targetIndex >= m_layers.size()) {
        return false;
    }

    const int targetLayerId = m_layers.at(targetIndex).id;
    if (targetLayerId == item->layerId()) {
        return false;
    }

    const ItemStackList before = itemStackSnapshot({item});
    const int targetHeight = nextHeight(targetLayerId);
    if (targetHeight < 0) {
        return false;
    }
    const ItemStackList after = {{item, targetLayerId, targetHeight}};

    const QString commandText =
        text.isEmpty() ? (delta > 0 ? tr("Move to the layer above") : tr("Move to the layer below")) : text;
    if (m_undoStack) {
        m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
    } else {
        applyItemStack(after);
    }
    return true;
}

void CanvasScene::applyItemStack(const ItemStackList& stack)
{
    QSet<int> affectedLayers;
    for (const ItemStack& entry : stack) {
        if (!entry.item) {
            continue;
        }
        if (!hasLayer(entry.layerId)) {
            entry.item->setLayerId(m_activeLayerId);
        } else {
            entry.item->setLayerId(entry.layerId);
        }
        entry.item->setHeight(qBound(0, entry.height, scene_constants::kMaxItemHeight));
        affectedLayers.insert(entry.item->layerId());
        applyItemStacking(entry.item);
        entry.item->update();
    }
    for (int layerId : std::as_const(affectedLayers)) {
        normalizeLayerHeights(layerId);
    }
    applyAllItemStacking();
    updateItemFlags();
    updateSceneExtent();
    update();
    emit contentChanged();
}

void CanvasScene::assignNewItemPlacement(CanvasItem* item)
{
    if (!item) {
        return;
    }

    if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId) {
        item->setLayerId(m_activeLayerId);
    }
    if (item->height() < 0 || item->height() > scene_constants::kMaxItemHeight ||
        heightUsedByOther(item->layerId(), item->height(), item)) {
        const int height = nextHeight(item->layerId());
        item->setHeight(height >= 0 ? height : 0);
    }
    applyItemStacking(item);
}
