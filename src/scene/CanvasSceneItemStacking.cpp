#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "scene/ItemHeightPlanner.h"
#include "scene/SceneConstants.h"

#include <QSet>
#include <QUndoStack>
#include <algorithm>
#include <utility>

qreal CanvasScene::LayerBaseZ(int layerId) const
{
    const int index = LayerIndex(layerId);
    return index > 0 ? index * scene_constants::LAYER_Z_STRIDE : 0.0;
}

void CanvasScene::ApplyItemStacking(CanvasItem* item)
{
    if (!item) {
        return;
    }
    const int maxHeight = scene_constants::MAX_ITEM_HEIGHT;
    const int height = qBound(0, item->ZOrder(), maxHeight);
    if (height != item->ZOrder()) {
        item->SetZOrder(height);
    }
    item->setZValue(LayerBaseZ(item->LayerId()) + height);
}

void CanvasScene::ApplyAllItemStacking()
{
    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        ApplyItemStacking(item);
    }
}

int CanvasScene::NextHeight(int layerId) const
{
    return ItemHeightPlanner().NextHeight(CanvasItems(), layerId);
}

bool CanvasScene::LayerHeightsUnique(int layerId) const
{
    return ItemHeightPlanner().HeightsUnique(LayerItems(layerId));
}

bool CanvasScene::HeightUsedByOther(int layerId, int height, const CanvasItem* except) const
{
    return ItemHeightPlanner().HeightUsedByOther(LayerItems(layerId), height, except);
}

bool CanvasScene::NormalizeLayerHeights(int layerId)
{
    QList<CanvasItem*> items = LayerItems(layerId);
    if (items.isEmpty()) {
        return false;
    }

    QSet<int> seen;
    bool needsNormalization = false;
    for (const CanvasItem* item : std::as_const(items)) {
        if (item->ZOrder() < 0 || item->ZOrder() > scene_constants::MAX_ITEM_HEIGHT || seen.contains(item->ZOrder())) {
            needsNormalization = true;
            break;
        }
        seen.insert(item->ZOrder());
    }
    if (!needsNormalization) {
        return false;
    }

    ItemStackList before;
    before.reserve(items.size());
    for (CanvasItem* item : items) {
        before.append({item, item->LayerId(), item->ZOrder()});
    }

    std::stable_sort(items.begin(), items.end(), [](const CanvasItem* a, const CanvasItem* b) {
        if (a->ZOrder() != b->ZOrder()) {
            return a->ZOrder() < b->ZOrder();
        }
        return a->zValue() < b->zValue();
    });
    for (int i = 0; i < items.size(); ++i) {
        CanvasItem* item = items.at(i);
        item->SetZOrder(i + 1);
        ApplyItemStacking(item);
        item->update();
    }

    // 归一化是真实的状态修复，必须能被撤销；fromJson 加载期间的修复不算
    // 用户操作（m_restoringDocument 守卫），不往刚清空的撤销栈里压命令。
    if (m_undoStack && !m_restoringDocument) {
        ItemStackList after;
        after.reserve(items.size());
        for (CanvasItem* item : std::as_const(items)) {
            after.append({item, item->LayerId(), item->ZOrder()});
        }
        m_undoStack->push(new ItemStackCommand(this, before, after, tr("Repair item stacking")));
    }
    return true;
}

void CanvasScene::NormalizeAllHeights()
{
    for (const CanvasLayer& entry : std::as_const(m_layers)) {
        if (entry.id != BackgroundLayerId) {
            NormalizeLayerHeights(entry.id);
        }
    }
}

ItemStackList CanvasScene::ItemStackSnapshot(const QList<CanvasItem*>& items) const
{
    ItemStackList snapshot;
    for (CanvasItem* item : items) {
        if (item) {
            snapshot.append({item, item->LayerId(), item->ZOrder()});
        }
    }
    return snapshot;
}

bool CanvasScene::AdjustItemHeight(CanvasItem* item, int delta, const QString& text)
{
    if (!item || delta == 0 || !HasLayer(item->LayerId())) {
        return false;
    }

    const int current = item->ZOrder();
    const int target = qBound(0, current + delta, scene_constants::MAX_ITEM_HEIGHT);
    if (target == current) {
        return false;
    }

    CanvasItem* colliding = nullptr;
    const QList<CanvasItem*> peers = LayerItems(item->LayerId());
    for (CanvasItem* peer : peers) {
        if (peer != item && peer->ZOrder() == target) {
            colliding = peer;
            break;
        }
    }

    const ItemStackList before =
        ItemStackSnapshot(colliding ? QList<CanvasItem*>{item, colliding} : QList<CanvasItem*>{item});
    ItemStackList after;
    after.append({item, item->LayerId(), target});
    if (colliding) {
        after.append({colliding, colliding->LayerId(), current});
    }

    const QString commandText = text.isEmpty() ? (delta > 0 ? tr("Move item up") : tr("Move item down")) : text;
    if (m_undoStack) {
        m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
    } else {
        ApplyItemStack(after);
    }
    return true;
}

int CanvasScene::AdjustSelectedHeights(int delta)
{
    if (delta == 0) {
        return 0;
    }

    QList<CanvasItem*> targets;
    const QList<CanvasItem*> selected = SelectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (item && IsItemEditable(item)) {
            targets.append(item);
        }
    }
    if (targets.isEmpty()) {
        return 0;
    }

    // 多选整块移动「全或不动」：任一目标越出 0–MAX_ITEM_HEIGHT 边界，或目标
    // 的新高度撞上未选中的图元，就整体不动。否则逐个 swap 会出现「底部两项
    // 按下移，被互相换位顶到最上面」的错乱结果。
    if (targets.size() > 1) {
        QHash<int, QSet<CanvasItem*>> targetsByLayer;
        QHash<int, QSet<int>> newHeightsByLayer;
        for (CanvasItem* item : targets) {
            targetsByLayer[item->LayerId()].insert(item);
            newHeightsByLayer[item->LayerId()].insert(item->ZOrder() + delta);
        }
        bool blocked = false;
        for (auto it = targetsByLayer.constBegin(); it != targetsByLayer.constEnd(); ++it) {
            const QSet<int>& newHeights = newHeightsByLayer[it.key()];
            for (CanvasItem* peer : LayerItems(it.key())) {
                if (!peer) {
                    continue;
                }
                if (it.value().contains(peer)) {
                    const int targetHeight = peer->ZOrder() + delta;
                    if (targetHeight < 0 || targetHeight > scene_constants::MAX_ITEM_HEIGHT) {
                        blocked = true;
                        break;
                    }
                } else if (newHeights.contains(peer->ZOrder())) {
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
        const ItemStackList before = ItemStackSnapshot(targets);
        ItemStackList after;
        after.reserve(targets.size());
        for (CanvasItem* item : targets) {
            after.append({item, item->LayerId(), item->ZOrder() + delta});
        }
        const QString commandText = delta > 0 ? tr("Move items up") : tr("Move items down");
        if (m_undoStack) {
            m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
        } else {
            ApplyItemStack(after);
        }
        return targets.size();
    }

    // 单选保持原有「与相邻图元互换」语义。
    return AdjustItemHeight(targets.first(), delta) ? 1 : 0;
}

int CanvasScene::MoveSelectedItemsToAdjacentLayer(int delta)
{
    if (delta == 0) {
        return 0;
    }

    QList<CanvasItem*> targets;
    const QList<CanvasItem*> selected = SelectedCanvasItems();
    for (CanvasItem* item : selected) {
        if (item && IsItemEditable(item)) {
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
        if (MoveItemToAdjacentLayer(item, delta)) {
            ++changed;
        }
    }

    if (grouped) {
        m_undoStack->endMacro();
    }
    return changed;
}

bool CanvasScene::MoveItemToAdjacentLayer(CanvasItem* item, int delta, const QString& text)
{
    if (!item || delta == 0) {
        return false;
    }

    const int currentIndex = LayerIndex(item->LayerId());
    const int targetIndex = currentIndex + delta;
    if (currentIndex <= 0 || targetIndex <= 0 || targetIndex >= m_layers.size()) {
        return false;
    }

    const int targetLayerId = m_layers.at(targetIndex).id;
    if (targetLayerId == item->LayerId()) {
        return false;
    }

    const ItemStackList before = ItemStackSnapshot({item});
    const int targetHeight = NextHeight(targetLayerId);
    if (targetHeight < 0) {
        return false;
    }
    const ItemStackList after = {{item, targetLayerId, targetHeight}};

    const QString commandText =
        text.isEmpty() ? (delta > 0 ? tr("Move to the layer above") : tr("Move to the layer below")) : text;
    if (m_undoStack) {
        m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
    } else {
        ApplyItemStack(after);
    }
    return true;
}

void CanvasScene::ApplyItemStack(const ItemStackList& stack)
{
    QSet<int> affectedLayers;
    for (const ItemStack& entry : stack) {
        if (!entry.item) {
            continue;
        }
        if (!HasLayer(entry.layerId)) {
            entry.item->SetLayerId(m_activeLayerId);
        } else {
            entry.item->SetLayerId(entry.layerId);
        }
        entry.item->SetZOrder(qBound(0, entry.height, scene_constants::MAX_ITEM_HEIGHT));
        affectedLayers.insert(entry.item->LayerId());
        ApplyItemStacking(entry.item);
        entry.item->update();
    }
    for (int layerId : std::as_const(affectedLayers)) {
        NormalizeLayerHeights(layerId);
    }
    ApplyAllItemStacking();
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    emit contentChanged();
}

void CanvasScene::AssignNewItemPlacement(CanvasItem* item)
{
    if (!item) {
        return;
    }

    if (!HasLayer(item->LayerId()) || item->LayerId() == BackgroundLayerId) {
        item->SetLayerId(m_activeLayerId);
    }
    if (item->ZOrder() < 0 || item->ZOrder() > scene_constants::MAX_ITEM_HEIGHT ||
        HeightUsedByOther(item->LayerId(), item->ZOrder(), item)) {
        const int height = NextHeight(item->LayerId());
        item->SetZOrder(height >= 0 ? height : 0);
    }
    ApplyItemStacking(item);
}
