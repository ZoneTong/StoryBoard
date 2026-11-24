#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "ui/panels/layers/LayerPanel.h"
#include "ui/panels/layers/LayerPanelView.h"

#include <QSet>
#include <QSignalBlocker>
#include <algorithm>

namespace {

QList<int> SortedUnique(QList<int> ids)
{
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

QList<CanvasItem*> LayerPanel::SelectedRowItems() const
{
    QList<CanvasItem*> items;
    for (auto entry = m_itemRows.constBegin(); entry != m_itemRows.constEnd(); ++entry) {
        QListWidgetItem* row = entry.key();
        if (!row || !row->isSelected()) {
            continue;
        }
        CanvasItem* item = entry.value().data();
        if (item) {
            items.append(item);
        }
    }
    return items;
}

QList<int> LayerPanel::PreferredLayerIdsFromSelection() const
{
    // 选中图元行也算“用户关注的图层”，否则删层 / 上下移按钮会误落到别的图层。
    QList<int> ids = SelectedLayerIds();
    for (auto entry = m_itemRows.constBegin(); entry != m_itemRows.constEnd(); ++entry) {
        QListWidgetItem* row = entry.key();
        CanvasItem* item = entry.value().data();
        if (row && item && row->isSelected() && item->LayerId() > CanvasScene::BackgroundLayerId) {
            ids.append(item->LayerId());
        }
    }
    return SortedUnique(ids);
}

void LayerPanel::ClearItemRowSelection()
{
    const QSignalBlocker blocker(m_list);
    for (auto entry = m_itemRows.constBegin(); entry != m_itemRows.constEnd(); ++entry) {
        if (entry.key()) {
            entry.key()->setSelected(false);
        }
    }
}

void LayerPanel::MirrorItemRowsFromScene()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }

    const QList<CanvasItem*> selected = m_scene->SelectedCanvasItems();
    QSet<CanvasItem*> selectedSet;
    for (CanvasItem* item : selected) {
        selectedSet.insert(item);
    }

    int completeGroup = 0;
    const bool complete = m_scene->SelectionIsCompleteGroup(&completeGroup);

    const QSignalBlocker blocker(m_list);
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* entry = m_list->item(row);
        if (!entry) {
            continue;
        }
        const int type = entry->data(ROLE_ROW_TYPE).toInt();
        if (type == ROW_ITEM) {
            CanvasItem* item = m_itemRows.value(entry).data();
            entry->setSelected(item && selectedSet.contains(item));
        } else if (type == ROW_GROUP) {
            entry->setSelected(complete && entry->data(Qt::UserRole).toInt() == completeGroup);
        }
    }
}

void LayerPanel::RevealSelectedItemRow()
{
    if (!m_list) {
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* entry = m_list->item(row);
        if (entry && entry->data(ROLE_ROW_TYPE).toInt() == ROW_ITEM && entry->isSelected()) {
            m_list->scrollToItem(entry, QAbstractItemView::EnsureVisible);
            return;
        }
    }
}

void LayerPanel::ApplyPanelSelectionToScene(const QList<CanvasItem*>& items)
{
    if (!m_scene) {
        return;
    }
    m_syncingScene = true;
    m_scene->clearSelection();
    for (CanvasItem* item : items) {
        // 隐藏 / 锁定图层里的图元行本就不可选；这里再兜一次，防止程序化勾选突破编辑权限。
        if (item && m_scene->IsItemEditable(item)) {
            item->setSelected(true);
        }
    }
    m_syncingScene = false;
    // 取消 m_syncingScene 后再走一遍正向同步，让活动层 / preferred / 行样式一次收敛。
    SyncSceneSelection();
}

void LayerPanel::OnSelectionChanged()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }

    const QList<CanvasItem*> rowItems = SelectedRowItems();
    if (!rowItems.isEmpty()) {
        ApplyPanelSelectionToScene(rowItems);
        return;
    }

    // 没有图元行选中：等价于在面板里清掉图元选择，画布选择跟着清空。
    // 点组行时 OnRowClicked 随后会把整组重新选中，所以这里直接清不会打断组选择。
    m_syncingScene = true;
    m_scene->clearSelection();
    m_syncingScene = false;
    ClearItemRowSelection();
    m_scene->SetPreferredLayerIds(PreferredLayerIdsFromSelection());
    m_view->RefreshRowStyles();
    m_view->RefreshButtons();
    m_view->RefreshSummary();
}

void LayerPanel::SyncSceneSelection()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }

    const QList<CanvasItem*> items = m_scene->SelectedCanvasItems();
    if (items.isEmpty()) {
        // 画布清空选择时只清图元行 / 组行，图层行的选择保留（原行为）。
        ClearItemRowSelection();
        if (m_list) {
            const QSignalBlocker blocker(m_list);
            for (int row = 0; row < m_list->count(); ++row) {
                QListWidgetItem* entry = m_list->item(row);
                if (entry && entry->data(ROLE_ROW_TYPE).toInt() == ROW_GROUP) {
                    entry->setSelected(false);
                }
            }
        }
        m_scene->SetPreferredLayerIds(PreferredLayerIdsFromSelection());
        m_view->RefreshRowStyles();
        m_view->RefreshButtons();
        m_view->RefreshSummary();
        return;
    }

    int completeGroup = 0;
    const bool complete = m_scene->SelectionIsCompleteGroup(&completeGroup);

    QSet<CanvasItem*> selectedSet;
    QList<int> ids;
    for (CanvasItem* item : items) {
        selectedSet.insert(item);
        ids.append(item->LayerId());
    }
    ids = SortedUnique(ids);
    const int topId = ids.last();

    m_syncingScene = true;
    if (!complete && m_scene->HasLayer(topId) && topId != CanvasScene::BackgroundLayerId &&
        topId != m_scene->ActiveLayerId()) {
        // 选中图元时自动把它的图层设为当前图层；完整组选中不改当前图层。
        m_scene->SetActiveLayer(topId);
    }

    int currentRow = -1;
    {
        const QSignalBlocker blocker(m_list);
        m_list->clearSelection();
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem* entry = m_list->item(row);
            if (!entry) {
                continue;
            }
            const int type = entry->data(ROLE_ROW_TYPE).toInt();
            const int id = entry->data(Qt::UserRole).toInt();
            if (type == ROW_LAYER) {
                // 完整组选中时不再顺带选图层行，面板焦点停在组行。
                if (!complete && ids.contains(id)) {
                    entry->setSelected(true);
                    if (id == topId) {
                        currentRow = row;
                    }
                }
            } else if (type == ROW_GROUP) {
                if (complete && id == completeGroup) {
                    entry->setSelected(true);
                    currentRow = row;
                }
            } else if (m_itemRows.contains(entry)) {
                CanvasItem* item = m_itemRows.value(entry).data();
                if (item && selectedSet.contains(item)) {
                    entry->setSelected(true);
                }
            }
        }
        if (currentRow >= 0) {
            m_list->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);
        }
    }
    m_syncingScene = false;

    // 面板空间有限，滚到首个被选中的图元行；组行整组选中时同样落到首个成员。
    RevealSelectedItemRow();
    m_scene->SetPreferredLayerIds(PreferredLayerIdsFromSelection());
    m_view->RefreshRowStyles();
    m_view->RefreshButtons();
    m_view->RefreshSummary();
}
