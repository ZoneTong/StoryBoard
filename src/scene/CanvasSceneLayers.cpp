#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "scene/ItemHeightPlanner.h"
#include "scene/SceneConstants.h"

#include <QUndoStack>
#include <algorithm>
#include <utility>

void CanvasScene::updateItemFlags()
{
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        applyItemInteraction(item);
    }
}

void CanvasScene::applyItemInteraction(CanvasItem* item)
{
    if (!item) {
        return;
    }

    const bool layerVisible = isLayerVisible(item->layerId());
    const bool editable = m_mode == ToolMode::Edit && layerVisible && !isLayerLocked(item->layerId());

    item->setVisible(layerVisible);
    item->setFlag(QGraphicsItem::ItemIsSelectable, editable);
    item->setAcceptHoverEvents(editable);
    if (!editable) {
        item->setSelected(false);
    }
}

QList<CanvasLayer> CanvasScene::userLayers() const
{
    if (m_layers.size() <= 1) {
        return {};
    }
    return m_layers.mid(1);
}

bool CanvasScene::hasLayer(int layerId) const
{
    return layerIndex(layerId) >= 0;
}

CanvasLayer CanvasScene::layer(int layerId) const
{
    for (const CanvasLayer& entry : m_layers) {
        if (entry.id == layerId) {
            return entry;
        }
    }
    return {};
}

int CanvasScene::layerIndex(int layerId) const
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers.at(i).id == layerId) {
            return i;
        }
    }
    return -1;
}

int CanvasScene::layerItemCount(int layerId) const
{
    int count = 0;
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (item->layerId() == layerId) {
            ++count;
        }
    }
    return count;
}

QList<CanvasItem*> CanvasScene::layerItems(int layerId) const
{
    QList<CanvasItem*> result;
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (item->layerId() == layerId) {
            result.append(item);
        }
    }
    return result;
}

bool CanvasScene::isLayerVisible(int layerId) const
{
    return hasLayer(layerId) && layer(layerId).visible;
}

bool CanvasScene::setLayerVisible(int layerId, bool visible)
{
    const int index = layerIndex(layerId);
    if (index < 0) {
        return false;
    }

    if (m_layers.at(index).visible == visible) {
        return true;
    }

    bool activeChanged = false;
    if (!visible && layerId == m_activeLayerId && layerId != BackgroundLayerId) {
        // The active layer is always visible. Hiding it moves the selection to
        // the nearest editable layer instead of leaving an invisible active
        // layer that cannot be edited.
        int fallbackIndex = -1;
        for (int distance = 1; distance < m_layers.size(); ++distance) {
            const int below = index - distance;
            if (below > 0) {
                fallbackIndex = below;
                break;
            }
            const int above = index + distance;
            if (above < m_layers.size()) {
                fallbackIndex = above;
                break;
            }
        }
        if (fallbackIndex < 0) {
            return false;
        }

        m_layers[fallbackIndex].visible = true;
        m_activeLayerId = m_layers.at(fallbackIndex).id;
        activeChanged = true;
    }

    m_layers[index].visible = visible;
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    if (activeChanged) {
        emit activeLayerChanged(m_activeLayerId);
    }
    return true;
}

bool CanvasScene::isLayerLocked(int layerId) const
{
    return hasLayer(layerId) && layer(layerId).locked;
}

bool CanvasScene::setAllLayersVisible(bool visible)
{
    bool changed = false;
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers.at(i).visible != visible) {
            m_layers[i].visible = visible;
            changed = true;
        }
    }
    if (!changed) {
        return true;
    }

    // 表头勾选框是批量浏览操作：允许一次隐藏全部图层（包括当前图层），
    // 不套用单行勾选框“至少保留一个可见用户图层”的限制。
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    return true;
}

bool CanvasScene::setLayerLocked(int layerId, bool locked)
{
    // 背景层始终锁定：它只承载背景，没有可编辑图元，也不允许解锁。
    if (layerId == BackgroundLayerId) {
        return false;
    }

    const int index = layerIndex(layerId);
    if (index < 0) {
        return false;
    }
    if (m_layers.at(index).locked == locked) {
        return true;
    }

    m_layers[index].locked = locked;
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    return true;
}

bool CanvasScene::isActiveLayerWritable() const
{
    return m_activeLayerId != BackgroundLayerId && hasLayer(m_activeLayerId) && isLayerVisible(m_activeLayerId) &&
           !isLayerLocked(m_activeLayerId);
}

bool CanvasScene::isItemEditable(const CanvasItem* item) const
{
    // 需求变更后的口径：编辑模式下，只要图元所在图层可见且未锁定就能编辑，
    // 与“当前图层”无关。
    return item && item->scene() == this && m_mode == ToolMode::Edit && isLayerVisible(item->layerId()) &&
           !isLayerLocked(item->layerId());
}

void CanvasScene::setActiveLayer(int layerId)
{
    if (layerId == BackgroundLayerId || !hasLayer(layerId)) {
        return;
    }

    bool visibilityChanged = false;
    if (!isLayerVisible(layerId)) {
        for (CanvasLayer& entry : m_layers) {
            if (entry.id == layerId) {
                entry.visible = true;
                break;
            }
        }
        visibilityChanged = true;
    }

    if (layerId == m_activeLayerId) {
        if (visibilityChanged) {
            updateItemFlags();
            updateSceneExtent();
            update();
            emit layersChanged();
            emit contentChanged();
        }
        return;
    }

    m_activeLayerId = layerId;
    updateItemFlags();
    updateSceneExtent();
    update();
    if (visibilityChanged) {
        emit layersChanged();
    }
    emit activeLayerChanged(m_activeLayerId);
    if (visibilityChanged) {
        emit contentChanged();
    }
}

int CanvasScene::createLayer(const QString& name)
{
    const int id = m_nextLayerId++;
    CanvasLayer entry;
    entry.id = id;
    entry.name = name.trimmed().isEmpty() ? QStringLiteral("图层 %1").arg(id) : name.trimmed();
    m_layers.append(entry);
    m_activeLayerId = id;
    applyAllItemStacking();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    return id;
}

bool CanvasScene::addLayerWithUndo(const QString& name)
{
    const LayerStackState before = layerState();
    const int id = m_nextLayerId++;
    CanvasLayer entry;
    entry.id = id;
    entry.name = name.trimmed().isEmpty() ? QStringLiteral("图层 %1").arg(id) : name.trimmed();

    LayerStackState after = before;
    after.layers.append(entry);
    after.activeLayerId = id;
    const QString text = tr("New layer");

    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

bool CanvasScene::removeLayerWithUndo(int layerId, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    const int index = layerIndex(layerId);
    if (index <= 0) {
        return fail(tr("The background layer cannot be deleted."));
    }
    if (userLayerCount() <= 1) {
        return fail(tr("At least one layer must remain."));
    }
    if (layerItemCount(layerId) > 0) {
        return fail(tr("This layer still contains items. Delete or move them first."));
    }

    const LayerStackState before = layerState();
    LayerStackState after = before;
    after.layers.removeAt(index);
    if (after.activeLayerId == layerId) {
        const int fallbackIndex = qBound(1, index - 1, static_cast<int>(after.layers.size()) - 1);
        after.activeLayerId = after.layers.at(fallbackIndex).id;
    }

    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, tr("Delete layer")));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

bool CanvasScene::moveLayerWithUndo(int layerId, int delta)
{
    const int index = layerIndex(layerId);
    if (index <= 0 || delta == 0) {
        return false;
    }

    const int target = index + delta;
    if (target <= 0 || target >= m_layers.size()) {
        return false;
    }

    const LayerStackState before = layerState();
    LayerStackState after = before;
    after.layers.swapItemsAt(index, target);

    const QString text = delta > 0 ? tr("Move layer up") : tr("Move layer down");
    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

bool CanvasScene::renameLayerWithUndo(int layerId, const QString& name)
{
    const int index = layerIndex(layerId);
    if (index <= 0) {
        return false;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_layers.at(index).name == trimmed) {
        return false;
    }

    const LayerStackState before = layerState();
    LayerStackState after = before;
    after.layers[index].name = trimmed;

    const QString text = tr("Rename layer");
    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

LayerStackState CanvasScene::layerState() const
{
    LayerStackState state;
    state.layers = m_layers;
    state.activeLayerId = m_activeLayerId;
    return state;
}

void CanvasScene::applyLayerState(const LayerStackState& state)
{
    applyLayerStateInternal(sanitizedLayerState(state));
}

LayerStackState CanvasScene::sanitizedLayerState(const LayerStackState& state) const
{
    LayerStackState result;
    bool backgroundVisible = true;
    for (const CanvasLayer& source : state.layers) {
        if (source.id == BackgroundLayerId) {
            backgroundVisible = source.visible;
            break;
        }
    }
    result.layers.append({BackgroundLayerId, scene_constants::kBackgroundLayerName, backgroundVisible, true});

    QSet<int> used;
    used.insert(BackgroundLayerId);
    for (const CanvasLayer& source : state.layers) {
        if (source.id <= 0 || used.contains(source.id)) {
            continue;
        }
        CanvasLayer entry = source;
        if (entry.name.trimmed().isEmpty()) {
            entry.name = QStringLiteral("图层 %1").arg(entry.id);
        }
        result.layers.append(entry);
        used.insert(entry.id);
    }

    if (result.layers.size() == 1) {
        int fallbackId = scene_constants::kFirstUserLayerId;
        while (used.contains(fallbackId)) {
            ++fallbackId;
        }
        result.layers.append({fallbackId, QStringLiteral("图层 %1").arg(fallbackId), true, false});
    }

    result.activeLayerId = state.activeLayerId;
    if (result.activeLayerId == BackgroundLayerId || !used.contains(result.activeLayerId)) {
        result.activeLayerId = result.layers.at(1).id;
    }
    for (CanvasLayer& entry : result.layers) {
        if (entry.id == result.activeLayerId) {
            entry.visible = true;
            break;
        }
    }
    return result;
}

void CanvasScene::applyLayerStateInternal(const LayerStackState& state, bool preserveLock)
{
    const QList<CanvasLayer> previousLayers = m_layers;
    m_layers = state.layers;
    m_activeLayerId = state.activeLayerId;

    // 锁定状态刻意不进入撤销栈：撤销/重做图层操作（新建、删除、排序、显隐）
    // 不应该顺带改变某一层的锁定。这里用同 id 的旧快照把 locked 覆盖回来；
    // 但从画布载入时要保留文件里的锁定状态，因此传入 preserveLock=false。
    if (preserveLock) {
        for (CanvasLayer& entry : m_layers) {
            for (const CanvasLayer& previous : previousLayers) {
                if (previous.id == entry.id) {
                    entry.locked = previous.locked;
                    break;
                }
            }
        }
    }

    for (CanvasLayer& entry : m_layers) {
        if (entry.id == m_activeLayerId && !entry.visible) {
            entry.visible = true;
        }
    }

    for (const CanvasLayer& entry : m_layers) {
        m_nextLayerId = qMax(m_nextLayerId, entry.id + 1);
    }

    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (!hasLayer(item->layerId())) {
            item->setLayerId(m_activeLayerId);
        }
    }
    normalizeAllHeights();
    applyAllItemStacking();
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
}

void CanvasScene::ensureLayerState()
{
    const LayerStackState fixed = sanitizedLayerState(layerState());
    if (fixed.layers != m_layers || fixed.activeLayerId != m_activeLayerId) {
        applyLayerStateInternal(fixed, false);
    }
}

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

    const bool grouped = m_undoStack && targets.size() > 1;
    if (grouped) {
        m_undoStack->beginMacro(delta > 0 ? tr("Move item up") : tr("Move item down"));
    }

    int changed = 0;
    for (CanvasItem* item : targets) {
        if (adjustItemHeight(item, delta)) {
            ++changed;
        }
    }

    if (grouped) {
        m_undoStack->endMacro();
    }
    return changed;
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

void CanvasScene::setPreferredLayerIds(const QList<int>& ids)
{
    if (m_preferredLayerIds == ids) {
        return;
    }
    m_preferredLayerIds = ids;
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
