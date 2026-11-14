#include "LayerPanelView.h"

#include "CanvasScene.h"
#include "LayerPanel.h"
#include "LayerRow.h"

#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QToolButton>

LayerPanelView::LayerPanelView(CanvasScene* scene, QListWidget* list, QCheckBox* allVisibleBox, QToolButton* addButton,
                               QToolButton* removeButton, QToolButton* upButton, QToolButton* downButton,
                               QLabel* summaryLabel)
    : m_scene(scene), m_list(list), m_allVisibleBox(allVisibleBox), m_addButton(addButton),
      m_removeButton(removeButton), m_upButton(upButton), m_downButton(downButton), m_summaryLabel(summaryLabel)
{
}

int LayerPanelView::currentLayerId() const
{
    const QListWidgetItem* current = m_list ? m_list->currentItem() : nullptr;
    return current ? current->data(Qt::UserRole).toInt() : 0;
}

QList<int> LayerPanelView::selectedLayerIds() const
{
    QList<int> ids;
    if (!m_list) {
        return ids;
    }
    const QList<QListWidgetItem*> rows = m_list->selectedItems();
    for (const QListWidgetItem* row : rows) {
        const int id = row->data(Qt::UserRole).toInt();
        if (id > CanvasScene::BackgroundLayerId) {
            ids.append(id);
        }
    }
    return ids;
}

void LayerPanelView::refreshRowStyles()
{
    if (!m_list) {
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* entry = m_list->item(row);
        auto* rowWidget = qobject_cast<LayerRow*>(m_list->itemWidget(entry));
        if (rowWidget) {
            rowWidget->applySelectedStyle(entry->isSelected());
        }
    }
}

void LayerPanelView::refreshAllVisibleState()
{
    if (!m_allVisibleBox || !m_scene) {
        return;
    }

    const QList<CanvasLayer> layers = m_scene->layers();
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

void LayerPanelView::refreshButtons()
{
    if (!m_scene) {
        return;
    }

    const int activeIndex = m_scene->layerIndex(m_scene->activeLayerId());
    QList<int> ids = selectedLayerIds();
    if (ids.isEmpty()) {
        if (m_scene->activeLayerId() > CanvasScene::BackgroundLayerId) {
            ids.append(m_scene->activeLayerId());
        }
    }

    int removable = 0;
    bool removeable = true;
    for (int id : ids) {
        const int index = m_scene->layerIndex(id);
        if (index <= 0 || m_scene->layerItemCount(id) > 0) {
            removeable = false;
            break;
        }
        ++removable;
    }

    m_addButton->setEnabled(true);
    m_removeButton->setEnabled(removeable && !ids.isEmpty() && m_scene->userLayerCount() - removable >= 1);
    m_upButton->setEnabled(activeIndex > 0 && activeIndex < m_scene->layerCount() - 1);
    m_downButton->setEnabled(activeIndex > 1);
}

void LayerPanelView::refreshSummary()
{
    if (!m_scene) {
        return;
    }

    const QList<int> ids = selectedLayerIds();
    QString text;
    if (ids.size() > 1) {
        text = LayerPanel::tr("%1 layer(s) selected (items can be multi-selected)\n").arg(ids.size());
    } else {
        const CanvasLayer active = m_scene->layer(m_scene->activeLayerId());
        text = LayerPanel::tr("Current: %1\n").arg(active.name);
    }
    text += LayerPanel::tr(
        "Checkbox = show/hide · lock button = lock\nIn edit mode, items on visible, unlocked layers can be edited");
    m_summaryLabel->setText(text);
}
