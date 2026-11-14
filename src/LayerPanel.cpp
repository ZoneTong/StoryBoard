#include "LayerPanel.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "I18n.h"
#include "LayerActions.h"
#include "LayerPanelView.h"
#include "LayerRow.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QToolButton* makeLayerButton(QWidget* parent, const QString& text, const QString& tip)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("LayerToolButton"));
    button->setText(text);
    button->setToolTip(tip);
    button->setFixedSize(32, 27);
    return button;
}

} // namespace

LayerPanel::LayerPanel(CanvasScene* scene, QWidget* parent) : QWidget(parent), m_scene(scene)
{
    setObjectName(QStringLiteral("LayerPanel"));
    setMinimumWidth(228);

    m_actions = new LayerActions(m_scene, this);
    connect(m_actions, &LayerActions::statusMessage, this, &LayerPanel::statusMessage);
    connect(m_actions, &LayerActions::refreshRequested, this, &LayerPanel::scheduleRefresh);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(7);

    auto* caption = new QLabel(tr("Top layer first · multi-select"), this);
    caption->setObjectName(QStringLiteral("LayerCaption"));
    layout->addWidget(caption);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("LayerHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(5);

    m_allVisibleBox = new QCheckBox(header);
    m_allVisibleBox->setObjectName(QStringLiteral("LayerVisibleAllBox"));
    m_allVisibleBox->setTristate(true);
    m_allVisibleBox->setFocusPolicy(Qt::NoFocus);
    m_allVisibleBox->setToolTip(tr("Show or hide all layers"));
    headerLayout->addWidget(m_allVisibleBox);

    auto* nameHeader = new QLabel(tr("Layer"), header);
    nameHeader->setObjectName(QStringLiteral("LayerNameHeader"));
    headerLayout->addWidget(nameHeader, 1);

    auto* lockHeader = new QLabel(tr("Lock"), header);
    lockHeader->setObjectName(QStringLiteral("LayerLockHeader"));
    lockHeader->setFixedWidth(30);
    lockHeader->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(lockHeader);
    header->setFixedHeight(22);
    layout->addWidget(header);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("LayerList"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_list, 1);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(5);
    m_addButton = makeLayerButton(this, QStringLiteral("+"), tr("New layer (placed on top)"));
    m_removeButton = makeLayerButton(this, QStringLiteral("−"), tr("Delete the selected empty layers (multi-select)"));
    m_upButton = makeLayerButton(this, QStringLiteral("↑"), tr("Move the current layer up one step"));
    m_downButton = makeLayerButton(this, QStringLiteral("↓"), tr("Move the current layer down one step"));
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_removeButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_upButton);
    buttonRow->addWidget(m_downButton);
    layout->addLayout(buttonRow);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("LayerHint"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_view = new LayerPanelView(m_scene, m_list, m_allVisibleBox, m_addButton, m_removeButton, m_upButton, m_downButton,
                                m_summaryLabel);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &LayerPanel::activateCurrentLayer);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() { m_view->refreshRowStyles(); });
    connect(m_allVisibleBox, &QCheckBox::clicked, this, [this]() {
        bool allVisible = true;
        const QList<CanvasLayer> layers = m_scene->layers();
        for (const CanvasLayer& layer : layers) {
            if (!layer.visible) {
                allVisible = false;
                break;
            }
        }
        m_actions->setAllLayersVisible(!allVisible);
    });
    connect(m_addButton, &QToolButton::clicked, this, [this]() { m_actions->addLayer(); });
    connect(m_removeButton, &QToolButton::clicked, this, &LayerPanel::removeSelectedLayers);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { m_actions->moveLayer(1); });
    connect(m_downButton, &QToolButton::clicked, this, [this]() { m_actions->moveLayer(-1); });

    // 场景信号可能来自行内控件自己的信号栈（勾选框、锁定按钮），直接 refresh
    // 会在 emit 者还被使用时销毁它，所以统一延后到下一个事件循环。
    connect(m_scene, &CanvasScene::layersChanged, this, &LayerPanel::scheduleRefresh);
    connect(m_scene, &CanvasScene::contentChanged, this, &LayerPanel::scheduleRefresh);
    connect(m_scene, &CanvasScene::activeLayerChanged, this, &LayerPanel::syncActiveLayer);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &LayerPanel::syncSceneSelection);

    refresh();
}

LayerRow* LayerPanel::createRow(const CanvasLayer& layer, int index, int itemCount)
{
    auto* row = new LayerRow(layer, index, itemCount);
    connect(row, &LayerRow::visibilityToggled, this, [this](int layerId, bool visible) {
        if (!m_refreshing) {
            m_actions->setLayerVisible(layerId, visible);
        }
    });
    connect(row, &LayerRow::lockToggled, this, [this](int layerId, bool locked) {
        if (!m_refreshing) {
            m_actions->setLayerLocked(layerId, locked);
        }
    });
    connect(row, &LayerRow::renameRequested, this, [this](int layerId) { m_actions->renameLayer(this, layerId); });
    return row;
}

QList<int> LayerPanel::selectedLayerIds() const
{
    return m_view->selectedLayerIds();
}

void LayerPanel::scheduleRefresh()
{
    if (m_refreshPending) {
        return;
    }
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshPending = false;
        refresh();
    });
}

void LayerPanel::refresh()
{
    if (!m_scene || m_refreshing) {
        return;
    }

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    const int scroll = m_list->verticalScrollBar()->value();
    const QList<int> previouslySelected = selectedLayerIds();
    const int activeId = m_scene->activeLayerId();
    const int previousCurrent = m_view->currentLayerId();

    m_list->clear();

    // 列表顶部是最高层；背景固定在最后一行且不可选中。
    const QList<CanvasLayer> layers = m_scene->layers();
    for (int i = layers.size() - 1; i >= 1; --i) {
        const CanvasLayer layer = layers.at(i);
        const int count = m_scene->layerItemCount(layer.id);
        auto* row = new QListWidgetItem(m_list);
        row->setData(Qt::UserRole, layer.id);
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        row->setSizeHint(QSize(0, 30));
        row->setToolTip(
            tr("Layer %1 · %2 items\n%3%4\nIn edit mode, items on visible, unlocked layers can be edited")
                .arg(i)
                .arg(count)
                .arg(layer.visible ? tr("Visible") : tr("Hidden"), layer.locked ? tr(" · Locked") : QString()));
        m_list->setItemWidget(row, createRow(layer, i, count));
    }

    auto* backgroundRow = new QListWidgetItem(m_list);
    backgroundRow->setData(Qt::UserRole, CanvasScene::BackgroundLayerId);
    backgroundRow->setFlags(Qt::ItemIsEnabled);
    backgroundRow->setSizeHint(QSize(0, 30));
    backgroundRow->setToolTip(tr("The background is always the bottom layer, cannot hold items, and is locked by "
                                 "default.\nUse the checkbox to show or hide it."));
    m_list->setItemWidget(backgroundRow, createRow(m_scene->layer(CanvasScene::BackgroundLayerId), 0, 0));

    // 先恢复当前行（不改变选择），再恢复多选。
    int currentRow = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        const int id = m_list->item(row)->data(Qt::UserRole).toInt();
        if (id == activeId) {
            currentRow = row;
        }
        if (previouslySelected.contains(id) || (previouslySelected.isEmpty() && id == previousCurrent)) {
            m_list->item(row)->setSelected(true);
        }
    }
    if (currentRow < 0) {
        for (int row = 0; row < m_list->count(); ++row) {
            if (m_list->item(row)->data(Qt::UserRole).toInt() == previousCurrent) {
                currentRow = row;
                break;
            }
        }
    }
    if (currentRow >= 0) {
        m_list->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);
    }

    if (currentRow >= 0) {
        if (selectedLayerIds().isEmpty() && activeId > CanvasScene::BackgroundLayerId) {
            m_list->item(currentRow)->setSelected(true);
        }
    }

    m_list->verticalScrollBar()->setValue(scroll);
    m_refreshing = false;

    m_scene->setPreferredLayerIds(selectedLayerIds());
    m_view->refreshRowStyles();
    m_view->refreshSummary();
    m_view->refreshButtons();
    m_view->refreshAllVisibleState();
}

void LayerPanel::activateCurrentLayer()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }
    const int id = m_view->currentLayerId();
    if (id > CanvasScene::BackgroundLayerId && m_scene->hasLayer(id)) {
        m_scene->setActiveLayer(id);
    }
    m_scene->setPreferredLayerIds(selectedLayerIds());
    m_view->refreshButtons();
    m_view->refreshSummary();
}

void LayerPanel::syncActiveLayer()
{
    if (!m_scene || m_refreshing) {
        return;
    }

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toInt() == m_scene->activeLayerId()) {
            m_list->setCurrentRow(row);
            break;
        }
    }
    m_refreshing = false;
    m_view->refreshSummary();
    m_view->refreshButtons();
}

void LayerPanel::syncSceneSelection()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }

    const QList<CanvasItem*> items = m_scene->selectedCanvasItems();
    if (items.isEmpty()) {
        return; // 清空选择时保持图层面板的选择，不做反向同步。
    }

    QList<int> ids;
    for (CanvasItem* item : items) {
        if (!ids.contains(item->layerId())) {
            ids.append(item->layerId());
        }
    }
    std::sort(ids.begin(), ids.end());
    const int topId = ids.last();

    m_syncingScene = true;
    if (m_scene->hasLayer(topId) && topId != CanvasScene::BackgroundLayerId && topId != m_scene->activeLayerId()) {
        // 选中图元时自动把它的图层设为当前图层。
        m_scene->setActiveLayer(topId);
    }

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    int currentRow = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        const int id = m_list->item(row)->data(Qt::UserRole).toInt();
        if (id == topId) {
            currentRow = row;
        }
    }
    if (currentRow >= 0) {
        m_list->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);
    }

    m_list->clearSelection();
    for (int row = 0; row < m_list->count(); ++row) {
        const int id = m_list->item(row)->data(Qt::UserRole).toInt();
        if (ids.contains(id)) {
            m_list->item(row)->setSelected(true);
        }
    }
    m_refreshing = false;
    m_syncingScene = false;

    m_scene->setPreferredLayerIds(ids);
    m_view->refreshRowStyles();
    m_view->refreshButtons();
    m_view->refreshSummary();
}

void LayerPanel::removeSelectedLayers()
{
    if (!m_scene) {
        return;
    }

    QList<int> ids = selectedLayerIds();
    if (ids.isEmpty()) {
        const int fallback = m_view->currentLayerId();
        if (fallback > CanvasScene::BackgroundLayerId) {
            ids.append(fallback);
        }
    }
    m_actions->removeLayer(this, ids);
}
