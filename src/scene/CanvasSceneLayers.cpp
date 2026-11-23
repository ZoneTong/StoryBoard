#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "scene/SceneConstants.h"

void CanvasScene::UpdateItemFlags()
{
    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        ApplyItemInteraction(item);
    }
}

void CanvasScene::ApplyItemInteraction(CanvasItem* item)
{
    if (!item) {
        return;
    }

    const bool layerVisible = IsLayerVisible(item->LayerId());
    const bool editable = m_mode == ToolMode::EDIT && layerVisible && !IsLayerLocked(item->LayerId());

    item->setVisible(layerVisible);
    item->setFlag(QGraphicsItem::ItemIsSelectable, editable);
    item->setAcceptHoverEvents(editable);
    if (!editable) {
        item->setSelected(false);
    }
}

QList<CanvasLayer> CanvasScene::UserLayers() const
{
    if (m_layerDomain.Layers().size() <= 1) {
        return {};
    }
    return m_layerDomain.Layers().mid(1);
}

bool CanvasScene::HasLayer(int layerId) const
{
    return LayerIndex(layerId) >= 0;
}

CanvasLayer CanvasScene::layer(int layerId) const
{
    for (const CanvasLayer& entry : m_layerDomain.Layers()) {
        if (entry.id == layerId) {
            return entry;
        }
    }
    return {};
}

int CanvasScene::LayerIndex(int layerId) const
{
    for (int i = 0; i < m_layerDomain.Layers().size(); ++i) {
        if (m_layerDomain.Layers().at(i).id == layerId) {
            return i;
        }
    }
    return -1;
}

int CanvasScene::LayerItemCount(int layerId) const
{
    int count = 0;
    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        if (item->LayerId() == layerId) {
            ++count;
        }
    }
    return count;
}

QList<CanvasItem*> CanvasScene::LayerItems(int layerId) const
{
    QList<CanvasItem*> result;
    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        if (item->LayerId() == layerId) {
            result.append(item);
        }
    }
    return result;
}

bool CanvasScene::IsLayerVisible(int layerId) const
{
    return HasLayer(layerId) && layer(layerId).visible;
}

bool CanvasScene::SetLayerVisible(int layerId, bool visible)
{
    const int index = LayerIndex(layerId);
    if (index < 0) {
        return false;
    }

    if (m_layerDomain.Layers().at(index).visible == visible) {
        return true;
    }

    bool activeChanged = false;
    if (!visible && layerId == m_layerDomain.ActiveLayerId() && layerId != BackgroundLayerId) {
        // The active layer is always visible. Hiding it moves the selection to
        // the nearest editable layer instead of leaving an invisible active
        // layer that cannot be edited.
        int fallbackIndex = -1;
        for (int distance = 1; distance < m_layerDomain.Layers().size(); ++distance) {
            const int below = index - distance;
            if (below > 0) {
                fallbackIndex = below;
                break;
            }
            const int above = index + distance;
            if (above < m_layerDomain.Layers().size()) {
                fallbackIndex = above;
                break;
            }
        }
        if (fallbackIndex < 0) {
            return false;
        }

        m_layerDomain.Layers()[fallbackIndex].visible = true;
        m_layerDomain.SetActiveLayerId(m_layerDomain.Layers().at(fallbackIndex).id);
        activeChanged = true;
    }

    m_layerDomain.Layers()[index].visible = visible;
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    if (activeChanged) {
        emit activeLayerChanged(m_layerDomain.ActiveLayerId());
    }
    return true;
}

bool CanvasScene::IsLayerLocked(int layerId) const
{
    return HasLayer(layerId) && layer(layerId).locked;
}

bool CanvasScene::SetAllLayersVisible(bool visible)
{
    bool changed = false;
    for (int i = 0; i < m_layerDomain.Layers().size(); ++i) {
        if (m_layerDomain.Layers().at(i).visible != visible) {
            m_layerDomain.Layers()[i].visible = visible;
            changed = true;
        }
    }
    if (!changed) {
        return true;
    }

    // 表头勾选框是批量浏览操作：允许一次隐藏全部图层（包括当前图层），
    // 不套用单行勾选框“至少保留一个可见用户图层”的限制。
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    return true;
}

bool CanvasScene::SetLayerLocked(int layerId, bool locked)
{
    // 背景层始终锁定：它只承载背景，没有可编辑图元，也不允许解锁。
    if (layerId == BackgroundLayerId) {
        return false;
    }

    const int index = LayerIndex(layerId);
    if (index < 0) {
        return false;
    }
    if (m_layerDomain.Layers().at(index).locked == locked) {
        return true;
    }

    m_layerDomain.Layers()[index].locked = locked;
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    return true;
}

bool CanvasScene::IsActiveLayerWritable() const
{
    return m_layerDomain.ActiveLayerId() != BackgroundLayerId && HasLayer(m_layerDomain.ActiveLayerId()) &&
           IsLayerVisible(m_layerDomain.ActiveLayerId()) && !IsLayerLocked(m_layerDomain.ActiveLayerId());
}

bool CanvasScene::IsItemEditable(const CanvasItem* item) const
{
    // 需求变更后的口径：编辑模式下，只要图元所在图层可见且未锁定就能编辑，
    // 与“当前图层”无关。
    return item && item->scene() == this && m_mode == ToolMode::EDIT && IsLayerVisible(item->LayerId()) &&
           !IsLayerLocked(item->LayerId());
}

void CanvasScene::SetActiveLayer(int layerId)
{
    if (layerId == BackgroundLayerId || !HasLayer(layerId)) {
        return;
    }

    bool visibilityChanged = false;
    if (!IsLayerVisible(layerId)) {
        for (CanvasLayer& entry : m_layerDomain.Layers()) {
            if (entry.id == layerId) {
                entry.visible = true;
                break;
            }
        }
        visibilityChanged = true;
    }

    if (layerId == m_layerDomain.ActiveLayerId()) {
        if (visibilityChanged) {
            UpdateItemFlags();
            UpdateSceneExtent();
            update();
            emit layersChanged();
            emit contentChanged();
        }
        return;
    }

    m_layerDomain.SetActiveLayerId(layerId);
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    if (visibilityChanged) {
        emit layersChanged();
    }
    emit activeLayerChanged(m_layerDomain.ActiveLayerId());
    if (visibilityChanged) {
        emit contentChanged();
    }
}

void CanvasScene::SetPreferredLayerIds(const QList<int>& ids)
{
    if (m_preferredLayerIds == ids) {
        return;
    }
    m_preferredLayerIds = ids;
}
