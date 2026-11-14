#include "CanvasItem.h"
#include "CanvasScene.h"
#include "scene/SceneConstants.h"

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

void CanvasScene::setPreferredLayerIds(const QList<int>& ids)
{
    if (m_preferredLayerIds == ids) {
        return;
    }
    m_preferredLayerIds = ids;
}
