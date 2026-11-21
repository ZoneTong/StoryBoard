#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "scene/SceneConstants.h"

#include <QSet>
#include <QUndoStack>

int CanvasScene::CreateLayer(const QString& name)
{
    const int id = m_nextLayerId++;
    CanvasLayer entry;
    entry.id = id;
    entry.name = name.trimmed().isEmpty() ? QStringLiteral("图层 %1").arg(id) : name.trimmed();
    m_layers.append(entry);
    m_activeLayerId = id;
    ApplyAllItemStacking();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    return id;
}

bool CanvasScene::AddLayerWithUndo(const QString& name)
{
    const LayerStackState before = LayerState();
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
        ApplyLayerStateInternal(after);
    }
    return true;
}

bool CanvasScene::RemoveLayerWithUndo(int layerId, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    const int index = LayerIndex(layerId);
    if (index <= 0) {
        return fail(tr("The background layer cannot be deleted."));
    }
    if (UserLayerCount() <= 1) {
        return fail(tr("At least one layer must remain."));
    }
    if (LayerItemCount(layerId) > 0) {
        return fail(tr("This layer still contains items. Delete or move them first."));
    }

    const LayerStackState before = LayerState();
    LayerStackState after = before;
    after.layers.removeAt(index);
    if (after.activeLayerId == layerId) {
        const int fallbackIndex = qBound(1, index - 1, static_cast<int>(after.layers.size()) - 1);
        after.activeLayerId = after.layers.at(fallbackIndex).id;
    }

    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, tr("Delete layer")));
    } else {
        ApplyLayerStateInternal(after);
    }
    return true;
}

bool CanvasScene::MoveLayerWithUndo(int layerId, int delta)
{
    const int index = LayerIndex(layerId);
    if (index <= 0 || delta == 0) {
        return false;
    }

    const int target = index + delta;
    if (target <= 0 || target >= m_layers.size()) {
        return false;
    }

    const LayerStackState before = LayerState();
    LayerStackState after = before;
    after.layers.swapItemsAt(index, target);

    const QString text = delta > 0 ? tr("Move layer up") : tr("Move layer down");
    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        ApplyLayerStateInternal(after);
    }
    return true;
}

bool CanvasScene::RenameLayerWithUndo(int layerId, const QString& name)
{
    const int index = LayerIndex(layerId);
    if (index <= 0) {
        return false;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_layers.at(index).name == trimmed) {
        return false;
    }

    const LayerStackState before = LayerState();
    LayerStackState after = before;
    after.layers[index].name = trimmed;

    const QString text = tr("Rename layer");
    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        ApplyLayerStateInternal(after);
    }
    return true;
}

LayerStackState CanvasScene::LayerState() const
{
    LayerStackState state;
    state.layers = m_layers;
    state.activeLayerId = m_activeLayerId;
    return state;
}

void CanvasScene::ApplyLayerState(const LayerStackState& state)
{
    ApplyLayerStateInternal(SanitizedLayerState(state));
}

LayerStackState CanvasScene::SanitizedLayerState(const LayerStackState& state) const
{
    LayerStackState result;
    bool backgroundVisible = true;
    for (const CanvasLayer& source : state.layers) {
        if (source.id == BackgroundLayerId) {
            backgroundVisible = source.visible;
            break;
        }
    }
    result.layers.append({BackgroundLayerId, scene_constants::BACKGROUND_LAYER_NAME, backgroundVisible, true});

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
        int fallbackId = scene_constants::FIRST_USER_LAYER_ID;
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

void CanvasScene::ApplyLayerStateInternal(const LayerStackState& state, bool preserveLock)
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

    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        if (!HasLayer(item->LayerId())) {
            item->SetLayerId(m_activeLayerId);
        }
    }
    NormalizeAllHeights();
    ApplyAllItemStacking();
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
}

void CanvasScene::EnsureLayerState()
{
    const LayerStackState fixed = SanitizedLayerState(LayerState());
    if (fixed.layers != m_layers || fixed.activeLayerId != m_activeLayerId) {
        ApplyLayerStateInternal(fixed, false);
    }
}
