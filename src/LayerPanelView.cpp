#include "LayerPanelView.h"

#include "CanvasScene.h"
#include "LayerGroupRow.h"
#include "LayerPanel.h"
#include "LayerRow.h"

#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

bool IsLayerRow(const QListWidgetItem* item)
{
    return item && item->data(LayerPanel::ROLE_ROW_TYPE).toInt() == LayerPanel::ROW_LAYER;
}

} // namespace

LayerPanelView::LayerPanelView(CanvasScene* scene, QListWidget* list, QCheckBox* allVisibleBox, QToolButton* addButton,
                               QToolButton* removeButton, QToolButton* upButton, QToolButton* downButton,
                               QLabel* summaryLabel)
    : m_scene(scene), m_list(list), m_allVisibleBox(allVisibleBox), m_addButton(addButton),
      m_removeButton(removeButton), m_upButton(upButton), m_downButton(downButton), m_summaryLabel(summaryLabel)
{
}

int LayerPanelView::CurrentLayerId() const
{
    const QListWidgetItem* current = m_list ? m_list->currentItem() : nullptr;
    if (!IsLayerRow(current)) {
        return 0;
    }
    return current->data(Qt::UserRole).toInt();
}

QList<int> LayerPanelView::SelectedLayerIds() const
{
    QList<int> ids;
    if (!m_list) {
        return ids;
    }
    const QList<QListWidgetItem*> rows = m_list->selectedItems();
    for (const QListWidgetItem* row : rows) {
        if (!IsLayerRow(row)) {
            continue;
        }
        const int id = row->data(Qt::UserRole).toInt();
        if (id > CanvasScene::BackgroundLayerId) {
            ids.append(id);
        }
    }
    return ids;
}

void LayerPanelView::RefreshRowStyles()
{
    if (!m_list) {
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* entry = m_list->item(row);
        auto* layerRow = qobject_cast<LayerRow*>(m_list->itemWidget(entry));
        if (layerRow) {
            layerRow->ApplySelectedStyle(entry->isSelected());
            continue;
        }
        auto* groupRow = qobject_cast<LayerGroupRow*>(m_list->itemWidget(entry));
        if (groupRow) {
            groupRow->ApplySelectedStyle(entry->isSelected());
        }
    }
}

void LayerPanelView::RefreshAllVisibleState()
{
    if (!m_allVisibleBox || !m_scene) {
        return;
    }

    const QList<CanvasLayer> layers = m_scene->Layers();
    int visibleCount = 0;
    for (const CanvasLayer& layer : layers) {
        if (layer.visible) {
            ++visibleCount;
        }
    }

    Qt::CheckState state = Qt::Unchecked;
    if (!layers.isEmpty() && visibleCount == layers.size()) {
        state = Qt::Checked;
    } else if (visibleCount > 0) {
        state = Qt::PartiallyChecked;
    }

    const QSignalBlocker blocker(m_allVisibleBox);
    m_allVisibleBox->setCheckState(state);
}

void LayerPanelView::RefreshButtons()
{
    if (!m_scene) {
        return;
    }

    const int activeIndex = m_scene->LayerIndex(m_scene->ActiveLayerId());
    QList<int> ids = SelectedLayerIds();
    if (ids.isEmpty()) {
        if (m_scene->ActiveLayerId() > CanvasScene::BackgroundLayerId) {
            ids.append(m_scene->ActiveLayerId());
        }
    }

    int removable = 0;
    bool removeable = true;
    for (int id : ids) {
        const int index = m_scene->LayerIndex(id);
        if (index <= 0 || m_scene->LayerItemCount(id) > 0) {
            removeable = false;
            break;
        }
        ++removable;
    }

    m_addButton->setEnabled(true);
    m_removeButton->setEnabled(removeable && !ids.isEmpty() && m_scene->UserLayerCount() - removable >= 1);
    m_upButton->setEnabled(activeIndex > 0 && activeIndex < m_scene->LayerCount() - 1);
    m_downButton->setEnabled(activeIndex > 1);
}

void LayerPanelView::RefreshSummary()
{
    if (!m_scene) {
        return;
    }

    const QList<int> ids = SelectedLayerIds();
    QString text;
    if (ids.size() > 1) {
        text = LayerPanel::tr("%1 layer(s) selected (items can be multi-selected)\n").arg(ids.size());
    } else {
        const CanvasLayer active = m_scene->layer(m_scene->ActiveLayerId());
        text = LayerPanel::tr("Current: %1\n").arg(active.name);
    }
    text += LayerPanel::tr(
        "Checkbox = show/hide · lock button = lock\nIn edit mode, items on visible, unlocked layers can be edited");
    m_summaryLabel->setText(text);
}
