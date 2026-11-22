#include "ui/panels/layers/LayerPanel.h"

#include "app/I18n.h"
#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "ui/panels/layers/LayerActions.h"
#include "ui/panels/layers/LayerGroupRow.h"
#include "ui/panels/layers/LayerPanelView.h"
#include "ui/panels/layers/LayerRow.h"

#include <QCheckBox>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QToolButton* MakeLayerButton(QWidget* parent, const QString& text, const QString& tip)
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
    connect(m_actions, &LayerActions::refreshRequested, this, &LayerPanel::ScheduleRefresh);

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
    // 图层行内部拖拽排序；组行 / 背景行不可拖动，拖拽由场景命令落撤销（§8.1）。
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->viewport()->installEventFilter(this);
    layout->addWidget(m_list, 1);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(5);
    m_addButton = MakeLayerButton(this, QStringLiteral("+"), tr("New layer (placed on top)"));
    m_removeButton = MakeLayerButton(this, QStringLiteral("−"), tr("Delete the selected empty layers (multi-select)"));
    m_upButton = MakeLayerButton(this, QStringLiteral("↑"), tr("Move the current layer up one step"));
    m_downButton = MakeLayerButton(this, QStringLiteral("↓"), tr("Move the current layer down one step"));
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

    connect(m_list, &QListWidget::itemSelectionChanged, this, &LayerPanel::ActivateCurrentLayer);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() { m_view->RefreshRowStyles(); });
    connect(m_list, &QListWidget::itemClicked, this, &LayerPanel::OnRowClicked);
    connect(m_allVisibleBox, &QCheckBox::clicked, this, [this]() {
        bool allVisible = true;
        const QList<CanvasLayer> layers = m_scene->Layers();
        for (const CanvasLayer& layer : layers) {
            if (!layer.visible) {
                allVisible = false;
                break;
            }
        }
        m_actions->SetAllLayersVisible(!allVisible);
    });
    connect(m_addButton, &QToolButton::clicked, this, [this]() { m_actions->AddLayer(); });
    connect(m_removeButton, &QToolButton::clicked, this, &LayerPanel::RemoveSelectedLayers);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { m_actions->MoveLayer(1); });
    connect(m_downButton, &QToolButton::clicked, this, [this]() { m_actions->MoveLayer(-1); });

    // 场景信号可能来自行内控件自己的信号栈（勾选框、锁定按钮），直接 refresh
    // 会在 emit 者还被使用时销毁它，所以统一延后到下一个事件循环。
    connect(m_scene, &CanvasScene::layersChanged, this, &LayerPanel::ScheduleRefresh);
    connect(m_scene, &CanvasScene::contentChanged, this, &LayerPanel::ScheduleRefresh);
    connect(m_scene, &CanvasScene::activeLayerChanged, this, &LayerPanel::SyncActiveLayer);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &LayerPanel::SyncSceneSelection);

    Refresh();
}

LayerRow* LayerPanel::CreateRow(const CanvasLayer& layer, int index, int itemCount)
{
    auto* row = new LayerRow(layer, index, itemCount);
    connect(row, &LayerRow::visibilityToggled, this, [this](int layerId, bool visible) {
        if (!m_refreshing) {
            m_actions->SetLayerVisible(layerId, visible);
        }
    });
    connect(row, &LayerRow::lockToggled, this, [this](int layerId, bool locked) {
        if (!m_refreshing) {
            m_actions->SetLayerLocked(layerId, locked);
        }
    });
    connect(row, &LayerRow::renameRequested, this, [this](int layerId) { m_actions->RenameLayer(this, layerId); });
    return row;
}

QListWidgetItem* LayerPanel::CreateGroupRow(int groupId)
{
    const QString name = m_scene->GroupName(groupId);
    const int count = m_scene->MemberCount(groupId);
    int depth = 0;
    for (int parent = m_scene->ParentGroup(groupId); parent > 0 && depth < 32; ++depth) {
        parent = m_scene->ParentGroup(parent);
    }
    auto* row = new QListWidgetItem(m_list);
    row->setData(Qt::UserRole, groupId);
    row->setData(ROLE_ROW_TYPE, ROW_GROUP);
    row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    row->setSizeHint(QSize(0, 28));
    row->setToolTip(tr("Group “%1” · %2 items").arg(name).arg(count));

    auto* groupRow = new LayerGroupRow(groupId, name, count, depth);
    connect(groupRow, &LayerGroupRow::renameRequested, this, [this](int id) { RenameGroup(id); });
    m_list->setItemWidget(row, groupRow);
    return row;
}

void LayerPanel::OnRowClicked(QListWidgetItem* item)
{
    if (!item || m_refreshing) {
        return;
    }
    if (item->data(ROLE_ROW_TYPE).toInt() != ROW_GROUP) {
        return;
    }
    const int groupId = item->data(Qt::UserRole).toInt();
    // 点组行只选中整组，不切换当前图层（§7.9）。
    m_scene->SelectGroupById(groupId);
}

void LayerPanel::RenameGroup(int groupId)
{
    if (!m_scene || groupId <= 0) {
        return;
    }

    const QString current = m_scene->GroupName(groupId);
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Rename group"), tr("Group name"), QLineEdit::Normal, current, &ok);
    if (!ok) {
        return;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::information(this, tr("Rename group"), tr("Group name cannot be empty."));
        return;
    }
    if (m_scene->RenameGroupWithUndo(groupId, trimmed)) {
        emit statusMessage(tr("Group renamed to “%1”").arg(trimmed));
        ScheduleRefresh();
    } else {
        QMessageBox::information(this, tr("Rename group"), tr("Group name is empty or already in use."));
    }
}

bool LayerPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list->viewport() && event->type() == QEvent::Drop) {
        return HandleLayerDrop(static_cast<QDropEvent*>(event));
    }
    return QWidget::eventFilter(watched, event);
}

int LayerPanel::OwnerLayerRow(int row) const
{
    for (int candidate = row; candidate >= 0; --candidate) {
        QListWidgetItem* entry = m_list->item(candidate);
        if (entry && entry->data(ROLE_ROW_TYPE).toInt() == ROW_LAYER) {
            return candidate;
        }
    }
    return -1;
}

bool LayerPanel::HandleLayerDrop(QDropEvent* event)
{
    if (!m_scene || m_refreshing) {
        return false;
    }

    QListWidgetItem* source = m_list->currentItem();
    if (!source || source->data(ROLE_ROW_TYPE).toInt() != ROW_LAYER) {
        event->ignore();
        return true;
    }
    const int sourceId = source->data(Qt::UserRole).toInt();
    if (sourceId <= CanvasScene::BackgroundLayerId) {
        event->ignore();
        return true;
    }

    int targetRow = m_list->indexAt(event->position().toPoint()).row();
    if (targetRow < 0) {
        event->ignore();
        return true;
    }
    targetRow = OwnerLayerRow(targetRow);
    QListWidgetItem* target = targetRow >= 0 ? m_list->item(targetRow) : nullptr;
    if (!target || target->data(ROLE_ROW_TYPE).toInt() != ROW_LAYER) {
        event->ignore();
        return true;
    }
    const int targetId = target->data(Qt::UserRole).toInt();
    // 背景层不能作为拖拽目标。
    if (targetId <= CanvasScene::BackgroundLayerId) {
        event->ignore();
        return true;
    }

    const int sourceIndex = m_scene->LayerIndex(sourceId);
    const int targetIndex = m_scene->LayerIndex(targetId);
    if (sourceIndex < 0 || targetIndex < 0 || sourceIndex == targetIndex) {
        event->ignore();
        return true;
    }

    if (m_scene->MoveLayerToIndexWithUndo(sourceId, targetIndex)) {
        event->accept();
        return true;
    }
    event->ignore();
    return true;
}

QList<int> LayerPanel::SelectedLayerIds() const
{
    return m_view->SelectedLayerIds();
}

void LayerPanel::ScheduleRefresh()
{
    if (m_refreshPending) {
        return;
    }
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshPending = false;
        Refresh();
    });
}

void LayerPanel::Refresh()
{
    if (!m_scene || m_refreshing) {
        return;
    }

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    const int scroll = m_list->verticalScrollBar()->value();
    const QList<int> previouslySelected = SelectedLayerIds();
    const int activeId = m_scene->ActiveLayerId();
    const int previousCurrent = m_view->CurrentLayerId();

    m_list->clear();

    // 列表顶部是最高层；背景固定在最后一行且不可选中。
    const QList<CanvasLayer> layers = m_scene->Layers();
    for (int i = layers.size() - 1; i >= 1; --i) {
        const CanvasLayer layer = layers.at(i);
        const int count = m_scene->LayerItemCount(layer.id);
        auto* row = new QListWidgetItem(m_list);
        row->setData(Qt::UserRole, layer.id);
        row->setData(ROLE_ROW_TYPE, ROW_LAYER);
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        row->setSizeHint(QSize(0, 30));
        row->setToolTip(
            tr("Layer %1 · %2 items\n%3%4\nIn edit mode, items on visible, unlocked layers can be edited")
                .arg(i)
                .arg(count)
                .arg(layer.visible ? tr("Visible") : tr("Hidden"), layer.locked ? tr(" · Locked") : QString()));
        m_list->setItemWidget(row, CreateRow(layer, i, count));

        // 组行紧跟所属图层行；先按嵌套层级、再按组名排序；背景层不出组行（§7.9）。
        QList<int> groupIds;
        for (int groupId : m_scene->GroupIds()) {
            if (m_scene->GroupLayerId(groupId) == layer.id) {
                groupIds.append(groupId);
            }
        }
        std::sort(groupIds.begin(), groupIds.end(), [this](int left, int right) {
            int leftDepth = 0;
            for (int parent = m_scene->ParentGroup(left); parent > 0 && leftDepth < 32; ++leftDepth) {
                parent = m_scene->ParentGroup(parent);
            }
            int rightDepth = 0;
            for (int parent = m_scene->ParentGroup(right); parent > 0 && rightDepth < 32; ++rightDepth) {
                parent = m_scene->ParentGroup(parent);
            }
            if (leftDepth != rightDepth) {
                return leftDepth < rightDepth;
            }
            return m_scene->GroupName(left).compare(m_scene->GroupName(right), Qt::CaseInsensitive) < 0;
        });
        for (int groupId : std::as_const(groupIds)) {
            CreateGroupRow(groupId);
        }
    }

    auto* backgroundRow = new QListWidgetItem(m_list);
    backgroundRow->setData(Qt::UserRole, CanvasScene::BackgroundLayerId);
    backgroundRow->setData(ROLE_ROW_TYPE, ROW_LAYER);
    backgroundRow->setFlags(Qt::ItemIsEnabled);
    backgroundRow->setSizeHint(QSize(0, 30));
    backgroundRow->setToolTip(tr("The background is always the bottom layer, cannot hold items, and is locked by "
                                 "default.\nUse the checkbox to show or hide it."));
    m_list->setItemWidget(backgroundRow, CreateRow(m_scene->layer(CanvasScene::BackgroundLayerId), 0, 0));

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
        if (SelectedLayerIds().isEmpty() && activeId > CanvasScene::BackgroundLayerId) {
            m_list->item(currentRow)->setSelected(true);
        }
    }

    m_list->verticalScrollBar()->setValue(scroll);
    m_refreshing = false;

    m_scene->SetPreferredLayerIds(SelectedLayerIds());
    m_view->RefreshRowStyles();
    m_view->RefreshSummary();
    m_view->RefreshButtons();
    m_view->RefreshAllVisibleState();
}

void LayerPanel::ActivateCurrentLayer()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }
    const int id = m_view->CurrentLayerId();
    if (id > CanvasScene::BackgroundLayerId && m_scene->HasLayer(id)) {
        m_scene->SetActiveLayer(id);
    }
    m_scene->SetPreferredLayerIds(SelectedLayerIds());
    m_view->RefreshButtons();
    m_view->RefreshSummary();
}

void LayerPanel::SyncActiveLayer()
{
    if (!m_scene || m_refreshing) {
        return;
    }

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toInt() == m_scene->ActiveLayerId()) {
            m_list->setCurrentRow(row);
            break;
        }
    }
    m_refreshing = false;
    m_view->RefreshSummary();
    m_view->RefreshButtons();
}

void LayerPanel::SyncSceneSelection()
{
    if (!m_scene || m_refreshing || m_syncingScene) {
        return;
    }

    const QList<CanvasItem*> items = m_scene->SelectedCanvasItems();
    if (items.isEmpty()) {
        return; // 清空选择时保持图层面板的选择，不做反向同步。
    }

    // 选中恰好是一个完整组时：只选中对应组行，不切换当前图层；混合选择不强制组行。
    int completeGroup = 0;
    if (m_scene->SelectionIsCompleteGroup(&completeGroup)) {
        m_syncingScene = true;
        const QSignalBlocker blocker(m_list);
        m_list->clearSelection();
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem* entry = m_list->item(row);
            if (entry->data(ROLE_ROW_TYPE).toInt() == ROW_GROUP && entry->data(Qt::UserRole).toInt() == completeGroup) {
                entry->setSelected(true);
                m_list->setCurrentRow(row, QItemSelectionModel::NoUpdate);
            }
        }
        m_syncingScene = false;
        m_view->RefreshRowStyles();
        return;
    }

    QList<int> ids;
    for (CanvasItem* item : items) {
        if (!ids.contains(item->LayerId())) {
            ids.append(item->LayerId());
        }
    }
    std::sort(ids.begin(), ids.end());
    const int topId = ids.last();

    m_syncingScene = true;
    if (m_scene->HasLayer(topId) && topId != CanvasScene::BackgroundLayerId && topId != m_scene->ActiveLayerId()) {
        // 选中图元时自动把它的图层设为当前图层。
        m_scene->SetActiveLayer(topId);
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

    m_scene->SetPreferredLayerIds(ids);
    m_view->RefreshRowStyles();
    m_view->RefreshButtons();
    m_view->RefreshSummary();
}

void LayerPanel::RemoveSelectedLayers()
{
    if (!m_scene) {
        return;
    }

    QList<int> ids = SelectedLayerIds();
    if (ids.isEmpty()) {
        const int fallback = m_view->CurrentLayerId();
        if (fallback > CanvasScene::BackgroundLayerId) {
            ids.append(fallback);
        }
    }
    m_actions->RemoveLayer(this, ids);
}
