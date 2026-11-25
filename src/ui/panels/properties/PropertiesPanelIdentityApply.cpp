// 属性面板的身份与叠放应用方法：重命名、目标图层、层内高度、同层同高互换，
// 以及图层 / 高度的相邻步进。与几何位置尺寸写入同属 PropertiesPanel，
// 按职责拆到本翻译单元，控制单文件行数。
#include "items/CanvasItem.h"
#include "items/ItemStack.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "scene/ItemTypeConversion.h"
#include "ui/panels/properties/PropertiesPanel.h"

void PropertiesPanel::ApplyItemType(const QString& kind)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    CanvasItem* item = items.first();
    if (ItemTypeConversion::TypeOf(item) == kind) {
        return;
    }
    // 换算与右键「转换为」共用 ConvertItemType：几何、起止角、描线样式原样搬过去，
    // 新图元挂在原叠放位置并接管选中，撤销时换回旧图元。
    if (!m_scene->ConvertItemType(item, kind)) {
        Refresh();
        return;
    }
    Refresh();
}

void PropertiesPanel::ApplyItemName(const QString& name)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    CanvasItem* item = items.first();
    const QString typed = name.trimmed();
    if (!typed.isEmpty() && !m_scene->IsItemNameAvailable(typed, item)) {
        emit statusMessage(tr("Another item already uses this name."));
        Refresh();
        return;
    }
    // 清空名称回到自动命名；未自定义时保持原显示名，不因打开面板就写成一个自定义名。
    QString value = typed;
    if (typed.isEmpty()) {
        value = m_scene->UniqueItemName(item->TypeDisplayName(), item);
    } else if (!item->HasCustomName() && typed == item->DisplayName()) {
        value.clear();
    }
    ApplyPropertyValues({{ItemProperty::NAME, value}}, tr("Rename item"), {item});
}

void PropertiesPanel::ApplyItemLayer(int layerId)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    ApplyItemStack(layerId, items.first()->ZOrder(), tr("Move item to another layer"));
}

void PropertiesPanel::ApplyItemHeight(int height)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene) {
        return;
    }
    ApplyItemStack(items.first()->LayerId(), height, tr("Change item height"));
}

void PropertiesPanel::ApplyItemStack(int layerId, int height, const QString& text)
{
    const QList<CanvasItem*> items = SelectedItems();
    if (items.size() != 1 || !m_scene || !m_scene->UndoStack()) {
        return;
    }
    CanvasItem* item = items.first();
    if (item->LayerId() == layerId && item->ZOrder() == height) {
        return;
    }
    // D6：组内成员不许单独换层，只有整组一起换层才放行。
    if (!m_scene->CanMoveItemToLayer(item, layerId)) {
        m_scene->NotifyGroupLayerBlocked();
        Refresh();
        return;
    }

    // 与属性窗一致：目标位置已有图元时，两条记录互换，不留同层同高度。
    CanvasItem* colliding = nullptr;
    for (CanvasItem* peer : m_scene->LayerItems(layerId)) {
        if (peer != item && peer->ZOrder() == height) {
            colliding = peer;
            break;
        }
    }

    ItemStackList before{{item, item->LayerId(), item->ZOrder()}};
    ItemStackList after{{item, layerId, height}};
    if (colliding) {
        before.append({colliding, colliding->LayerId(), colliding->ZOrder()});
        after.append({colliding, item->LayerId(), item->ZOrder()});
    }
    m_scene->UndoStack()->push(new ItemStackCommand(m_scene, before, after, text));
    Refresh();
}

void PropertiesPanel::AdjustLayer(int delta)
{
    if (m_scene) {
        const int count = m_scene->MoveSelectedItemsToAdjacentLayer(delta);
        statusMessage(count > 0 ? tr("Moved %n Item(s) to another layer.", nullptr, count)
                                : tr("No layer to move to in that direction."));
        Refresh();
    }
}

void PropertiesPanel::AdjustHeight(int delta)
{
    if (m_scene) {
        const int count = m_scene->AdjustSelectedHeights(delta);
        statusMessage(count > 0 ? tr("Adjusted the height of %n Item(s).", nullptr, count)
                                : tr("The height did not change."));
        Refresh();
    }
}
