#include "I18n.h"

#include "LayerPanel.h"

#include "BoardItem.h"
#include "BoardScene.h"
#include "IconFactory.h"

#include <algorithm>

#include <QCheckBox>
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

LayerPanel::LayerPanel(BoardScene* scene, QWidget* parent)
    : QWidget(parent)
    , m_scene(scene)
{
    setObjectName(QStringLiteral("LayerPanel"));
    setMinimumWidth(228);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(7);

    auto* caption = new QLabel(uiText(QStringLiteral("Top layer first · multi-select")), this);
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
    m_allVisibleBox->setToolTip(uiText(QStringLiteral("Show or hide all layers")));
    headerLayout->addWidget(m_allVisibleBox);

    auto* nameHeader = new QLabel(uiText(QStringLiteral("Layer")), header);
    nameHeader->setObjectName(QStringLiteral("LayerNameHeader"));
    nameHeader->setStyleSheet(QStringLiteral("color: rgb(96, 104, 120); font-weight: 600;"));
    headerLayout->addWidget(nameHeader, 1);

    auto* lockHeader = new QLabel(uiText(QStringLiteral("Lock")), header);
    lockHeader->setObjectName(QStringLiteral("LayerLockHeader"));
    lockHeader->setFixedWidth(30);
    lockHeader->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lockHeader->setStyleSheet(QStringLiteral("color: rgb(96, 104, 120); font-weight: 600;"));
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
    m_addButton = makeLayerButton(this, QStringLiteral("+"), uiText(QStringLiteral("New layer (placed on top)")));
    m_removeButton = makeLayerButton(this, QStringLiteral("−"), uiText(QStringLiteral("Delete the selected empty layers (multi-select)")));
    m_upButton = makeLayerButton(this, QStringLiteral("↑"), uiText(QStringLiteral("Move the current layer up one step")));
    m_downButton = makeLayerButton(this, QStringLiteral("↓"), uiText(QStringLiteral("Move the current layer down one step")));
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

    connect(m_list, &QListWidget::itemSelectionChanged, this, &LayerPanel::activateCurrentLayer);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() { updateRowStyles(); });
    connect(m_allVisibleBox, &QCheckBox::clicked, this, [this]() {
        bool allVisible = true;
        const QList<BoardLayer> layers = m_scene->layers();
        for (const BoardLayer& layer : layers) {
            if (!layer.visible) {
                allVisible = false;
                break;
            }
        }
        setAllLayersVisible(!allVisible);
    });
    connect(m_addButton, &QToolButton::clicked, this, &LayerPanel::addLayer);
    connect(m_removeButton, &QToolButton::clicked, this, &LayerPanel::removeLayer);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    connect(m_downButton, &QToolButton::clicked, this, [this]() { moveLayer(-1); });

    // 场景信号可能来自行内控件自己的信号栈（勾选框、锁定按钮），直接 refresh
    // 会在 emit 者还被使用时销毁它，所以统一延后到下一个事件循环。
    connect(m_scene, &BoardScene::layersChanged, this, &LayerPanel::scheduleRefresh);
    connect(m_scene, &BoardScene::contentChanged, this, &LayerPanel::scheduleRefresh);
    connect(m_scene, &BoardScene::activeLayerChanged, this, &LayerPanel::syncActiveLayer);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &LayerPanel::syncSceneSelection);

    refresh();
}

QWidget* LayerPanel::createLayerRow(const BoardLayer& layer, int index, int itemCount)
{
    auto* row = new QWidget;
    row->setObjectName(QStringLiteral("LayerRow"));

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(5);

    auto* visibleBox = new QCheckBox(row);
    visibleBox->setObjectName(QStringLiteral("LayerVisibleBox"));
    visibleBox->setFocusPolicy(Qt::NoFocus);
    visibleBox->setToolTip(uiText(QStringLiteral("Check to show this layer, uncheck to hide it")));
    visibleBox->setChecked(layer.visible);

    auto* nameLabel = new QLabel(row);
    nameLabel->setObjectName(QStringLiteral("LayerRowLabel"));
    QString text = (index == BoardScene::BackgroundLayerId)
        ? uiText(QStringLiteral("Background    (layer 0)"))
        : uiText(QStringLiteral("%1    %2 items")).arg(layer.name).arg(itemCount);
    if (!layer.visible)
        text += uiText(QStringLiteral("  · Hidden"));
    if (layer.locked)
        text += uiText(QStringLiteral("  · Locked"));
    nameLabel->setText(text);
    if (index == BoardScene::BackgroundLayerId)
        nameLabel->setStyleSheet(QStringLiteral("color: rgb(120, 128, 140);"));
    else if (layer.locked)
        nameLabel->setStyleSheet(QStringLiteral("color: rgb(96, 104, 120);"));
    else if (!layer.visible)
        nameLabel->setStyleSheet(QStringLiteral("color: rgb(150, 157, 169);"));

    if (index != BoardScene::BackgroundLayerId) {
        nameLabel->setProperty("layerId", layer.id);
        nameLabel->setCursor(Qt::IBeamCursor);
        nameLabel->setToolTip(uiText(QStringLiteral("Double-click to rename layer")));
        nameLabel->installEventFilter(this);
    }

    layout->addWidget(visibleBox);
    layout->addWidget(nameLabel, 1);

    if (index != BoardScene::BackgroundLayerId) {
        auto* lockButton = new QToolButton(row);
        lockButton->setObjectName(QStringLiteral("LayerLockButton"));
        lockButton->setCheckable(true);
        lockButton->setChecked(layer.locked);
        lockButton->setFocusPolicy(Qt::NoFocus);
        lockButton->setFixedSize(22, 22);
        lockButton->setIcon(makeIcon(IconId::Lock,
                                     layer.locked ? QColor(37, 99, 235) : QColor(110, 118, 132), 18));
        lockButton->setIconSize(QSize(18, 18));
        lockButton->setToolTip(uiText(QStringLiteral("Lock layer: items on it cannot be edited")));
        layout->addWidget(lockButton);
        connect(lockButton, &QToolButton::toggled, this,
                [this, layerId = layer.id](bool locked) { setLayerLocked(layerId, locked); });
    }

    connect(visibleBox, &QCheckBox::toggled, this,
            [this, layerId = layer.id](bool visible) { setLayerVisible(layerId, visible); });

    return row;
}

int LayerPanel::currentLayerId() const
{
    const QListWidgetItem* current = m_list->currentItem();
    return current ? current->data(Qt::UserRole).toInt() : 0;
}

QList<int> LayerPanel::selectedLayerIds() const
{
    QList<int> ids;
    const QList<QListWidgetItem*> rows = m_list->selectedItems();
    for (const QListWidgetItem* row : rows) {
        const int id = row->data(Qt::UserRole).toInt();
        if (id > BoardScene::BackgroundLayerId)
            ids.append(id);
    }
    return ids;
}

void LayerPanel::scheduleRefresh()
{
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshPending = false;
        refresh();
    });
}

void LayerPanel::refresh()
{
    if (!m_scene || m_refreshing)
        return;

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    const int scroll = m_list->verticalScrollBar()->value();
    const QList<int> previouslySelected = selectedLayerIds();
    const int activeId = m_scene->activeLayerId();
    const int previousCurrent = currentLayerId();

    m_list->clear();

    // 列表顶部是最高层；背景固定在最后一行且不可选中。
    const QList<BoardLayer> layers = m_scene->layers();
    for (int i = layers.size() - 1; i >= 1; --i) {
        const BoardLayer layer = layers.at(i);
        const int count = m_scene->layerItemCount(layer.id);
        auto* row = new QListWidgetItem(m_list);
        row->setData(Qt::UserRole, layer.id);
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        row->setSizeHint(QSize(0, 30));
        row->setToolTip(uiText(QStringLiteral("Layer %1 · %2 items\n%3%4\nIn edit mode, items on visible, unlocked layers can be edited"))
                            .arg(i)
                            .arg(count)
                            .arg(layer.visible ? uiText(QStringLiteral("Visible"))
                                               : uiText(QStringLiteral("Hidden")),
                                 layer.locked ? uiText(QStringLiteral(" · Locked")) : QString()));
        m_list->setItemWidget(row, createLayerRow(layer, i, count));
    }

    auto* backgroundRow = new QListWidgetItem(m_list);
    backgroundRow->setData(Qt::UserRole, BoardScene::BackgroundLayerId);
    backgroundRow->setFlags(Qt::ItemIsEnabled);
    backgroundRow->setSizeHint(QSize(0, 30));
    backgroundRow->setToolTip(uiText(QStringLiteral("The background is always the bottom layer, cannot hold items, and is locked by default.\nUse the checkbox to show or hide it.")));
    m_list->setItemWidget(backgroundRow,
                          createLayerRow(m_scene->layer(BoardScene::BackgroundLayerId), 0, 0));

    // 先恢复当前行（不改变选择），再恢复多选。
    int currentRow = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        const int id = m_list->item(row)->data(Qt::UserRole).toInt();
        if (id == activeId)
            currentRow = row;
        if (previouslySelected.contains(id) || (previouslySelected.isEmpty() && id == previousCurrent))
            m_list->item(row)->setSelected(true);
    }
    if (currentRow < 0) {
        for (int row = 0; row < m_list->count(); ++row) {
            if (m_list->item(row)->data(Qt::UserRole).toInt() == previousCurrent) {
                currentRow = row;
                break;
            }
        }
    }
    if (currentRow >= 0)
        m_list->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);

    if (currentRow >= 0) {
        if (selectedLayerIds().isEmpty() && activeId > BoardScene::BackgroundLayerId)
            m_list->item(currentRow)->setSelected(true);
    }

    m_list->verticalScrollBar()->setValue(scroll);
    m_refreshing = false;

    m_scene->setPreferredLayerIds(selectedLayerIds());
    updateRowStyles();
    updateSummary();
    updateButtons();
    updateAllVisibleState();
}

bool LayerPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::MouseButtonDblClick) {
        if (QWidget* widget = qobject_cast<QWidget*>(watched)) {
            if (widget->objectName() == QStringLiteral("LayerRowLabel")) {
                bool ok = false;
                const int layerId = widget->property("layerId").toInt(&ok);
                if (ok) {
                    renameLayer(layerId);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LayerPanel::activateCurrentLayer()
{
    if (!m_scene || m_refreshing || m_syncingScene)
        return;
    const int id = currentLayerId();
    if (id > BoardScene::BackgroundLayerId && m_scene->hasLayer(id))
        m_scene->setActiveLayer(id);
    m_scene->setPreferredLayerIds(selectedLayerIds());
    updateButtons();
    updateSummary();
}

void LayerPanel::syncActiveLayer()
{
    if (!m_scene || m_refreshing)
        return;

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toInt() == m_scene->activeLayerId()) {
            m_list->setCurrentRow(row);
            break;
        }
    }
    m_refreshing = false;
    updateSummary();
    updateButtons();
}

void LayerPanel::syncSceneSelection()
{
    if (!m_scene || m_refreshing || m_syncingScene)
        return;

    const QList<BoardItem*> items = m_scene->selectedBoardItems();
    if (items.isEmpty())
        return; // 清空选择时保持图层面板的选择，不做反向同步。

    QList<int> ids;
    for (BoardItem* item : items) {
        if (!ids.contains(item->layerId()))
            ids.append(item->layerId());
    }
    std::sort(ids.begin(), ids.end());
    const int topId = ids.last();

    m_syncingScene = true;
    if (m_scene->hasLayer(topId) && topId != BoardScene::BackgroundLayerId
        && topId != m_scene->activeLayerId()) {
        // 选中图元时自动把它的图层设为当前图层。
        m_scene->setActiveLayer(topId);
    }

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    int currentRow = -1;
    for (int row = 0; row < m_list->count(); ++row) {
        const int id = m_list->item(row)->data(Qt::UserRole).toInt();
        if (id == topId)
            currentRow = row;
    }
    if (currentRow >= 0)
        m_list->setCurrentRow(currentRow, QItemSelectionModel::NoUpdate);

    m_list->clearSelection();
    for (int row = 0; row < m_list->count(); ++row) {
        const int id = m_list->item(row)->data(Qt::UserRole).toInt();
        if (ids.contains(id))
            m_list->item(row)->setSelected(true);
    }
    m_refreshing = false;
    m_syncingScene = false;

    m_scene->setPreferredLayerIds(ids);
    updateRowStyles();
    updateButtons();
    updateSummary();
}

void LayerPanel::updateRowStyles()
{
    const QString selectedStyle = QStringLiteral(
        "QWidget#LayerRow { background-color: rgb(232, 238, 251);"
        " border-left: 3px solid rgb(37, 99, 235); border-radius: 4px; }");
    const QString normalStyle = QStringLiteral(
        "QWidget#LayerRow { background-color: transparent;"
        " border-left: 3px solid transparent; }");

    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* entry = m_list->item(row);
        QWidget* widget = m_list->itemWidget(entry);
        if (!widget)
            continue;
        widget->setStyleSheet(entry->isSelected() ? selectedStyle : normalStyle);
    }
}

void LayerPanel::setLayerVisible(int layerId, bool visible)
{
    if (!m_scene || m_refreshing)
        return;

    const QString layerName = (layerId == BoardScene::BackgroundLayerId)
        ? uiText(QStringLiteral("Background layer"))
        : m_scene->layer(layerId).name;
    const int oldActive = m_scene->activeLayerId();

    if (!m_scene->setLayerVisible(layerId, visible)) {
        emit statusMessage(uiText(QStringLiteral("The last visible layer cannot be hidden. Create a new layer first.")));
        scheduleRefresh();
        return;
    }

    if (layerId == BoardScene::BackgroundLayerId) {
        emit statusMessage(visible ? uiText(QStringLiteral("Background layer shown"))
                                   : uiText(QStringLiteral("Background layer hidden")));
    } else if (!visible && oldActive == layerId
               && m_scene->activeLayerId() != oldActive) {
        emit statusMessage(uiText(QStringLiteral("Layer \"%1\" hidden; the current layer switched to \"%2\""))
                               .arg(layerName, m_scene->layer(m_scene->activeLayerId()).name));
    } else {
        emit statusMessage(visible ? uiText(QStringLiteral("Layer \"%1\" shown")).arg(layerName)
                                   : uiText(QStringLiteral("Layer \"%1\" hidden")).arg(layerName));
    }
    scheduleRefresh();
}

void LayerPanel::setAllLayersVisible(bool visible)
{
    if (!m_scene)
        return;
    if (!m_scene->setAllLayersVisible(visible))
        return;
    emit statusMessage(visible ? uiText(QStringLiteral("All layers shown"))
                               : uiText(QStringLiteral("All layers hidden")));
    scheduleRefresh();
    updateAllVisibleState();
}

void LayerPanel::updateAllVisibleState()
{
    if (!m_allVisibleBox || !m_scene)
        return;

    const QList<BoardLayer> layers = m_scene->layers();
    int visibleCount = 0;
    for (const BoardLayer& layer : layers) {
        if (layer.visible)
            ++visibleCount;
    }

    Qt::CheckState state = Qt::Unchecked;
    if (!layers.isEmpty() && visibleCount == layers.size())
        state = Qt::Checked;
    else if (visibleCount > 0)
        state = Qt::PartiallyChecked;

    const QSignalBlocker blocker(m_allVisibleBox);
    m_allVisibleBox->setCheckState(state);
}

void LayerPanel::setLayerLocked(int layerId, bool locked)
{
    if (!m_scene || m_refreshing)
        return;

    const QString layerName = m_scene->layer(layerId).name;
    if (!m_scene->setLayerLocked(layerId, locked)) {
        emit statusMessage(uiText(QStringLiteral("The background layer is always locked.")));
        scheduleRefresh();
        return;
    }

    emit statusMessage(locked ? uiText(QStringLiteral("Locked layer \"%1\"; its items can no longer be edited")).arg(layerName)
                              : uiText(QStringLiteral("Unlocked layer \"%1\"")).arg(layerName));
    scheduleRefresh();
}

void LayerPanel::addLayer()
{
    if (!m_scene)
        return;
    m_scene->addLayerWithUndo();
    emit statusMessage(uiText(QStringLiteral("Layer created")));
}

void LayerPanel::removeLayer()
{
    if (!m_scene)
        return;

    QList<int> ids = selectedLayerIds();
    if (ids.isEmpty()) {
        const int fallback = currentLayerId();
        if (fallback > BoardScene::BackgroundLayerId)
            ids.append(fallback);
    }
    if (ids.isEmpty())
        return;

    for (int id : ids) {
        if (m_scene->layerItemCount(id) > 0) {
            QMessageBox::information(
                this, uiText(QStringLiteral("Delete layer")),
                uiText(QStringLiteral("Layer \"%1\" still contains items. Move or delete them first."))
                    .arg(m_scene->layer(id).name));
            return;
        }
    }
    if (m_scene->userLayerCount() - ids.size() < 1) {
        QMessageBox::information(this, uiText(QStringLiteral("Delete layer")),
                                 uiText(QStringLiteral("At least one user layer must remain.")));
        return;
    }

    for (int id : ids)
        m_scene->removeLayerWithUndo(id, nullptr);
    emit statusMessage(ids.size() > 1 ? uiText(QStringLiteral("%1 layer(s) deleted")).arg(ids.size())
                                      : uiText(QStringLiteral("Layer deleted")));
    scheduleRefresh();
}

void LayerPanel::renameLayer(int layerId)
{
    if (!m_scene || layerId <= BoardScene::BackgroundLayerId || !m_scene->hasLayer(layerId))
        return;

    const BoardLayer layer = m_scene->layer(layerId);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, uiText(QStringLiteral("Rename layer")), uiText(QStringLiteral("Layer name")),
        QLineEdit::Normal, layer.name, &ok);
    if (!ok)
        return;

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::information(this, uiText(QStringLiteral("Rename layer")),
                                 uiText(QStringLiteral("Layer name cannot be empty.")));
        return;
    }
    if (trimmed == layer.name)
        return;

    if (m_scene->renameLayerWithUndo(layerId, trimmed)) {
        emit statusMessage(uiText(QStringLiteral("Layer renamed to \"%1\"")).arg(trimmed));
        scheduleRefresh();
    }
}

void LayerPanel::moveLayer(int delta)
{
    if (!m_scene)
        return;
    const int id = m_scene->activeLayerId();
    if (m_scene->moveLayerWithUndo(id, delta)) {
        emit statusMessage(delta > 0 ? uiText(QStringLiteral("Layer moved up"))
                                     : uiText(QStringLiteral("Layer moved down")));
    }
}

void LayerPanel::updateButtons()
{
    if (!m_scene)
        return;

    const int activeIndex = m_scene->layerIndex(m_scene->activeLayerId());
    QList<int> ids = selectedLayerIds();
    if (ids.isEmpty()) {
        if (m_scene->activeLayerId() > BoardScene::BackgroundLayerId)
            ids.append(m_scene->activeLayerId());
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
    m_removeButton->setEnabled(removeable && !ids.isEmpty()
                               && m_scene->userLayerCount() - removable >= 1);
    m_upButton->setEnabled(activeIndex > 0 && activeIndex < m_scene->layerCount() - 1);
    m_downButton->setEnabled(activeIndex > 1);
}

void LayerPanel::updateSummary()
{
    if (!m_scene)
        return;

    const QList<int> ids = selectedLayerIds();
    QString text;
    if (ids.size() > 1) {
        text = uiText(QStringLiteral("%1 layer(s) selected (items can be multi-selected)\n")).arg(ids.size());
    } else {
        const BoardLayer active = m_scene->layer(m_scene->activeLayerId());
        text = uiText(QStringLiteral("Current: %1\n")).arg(active.name);
    }
    text += uiText(QStringLiteral("Checkbox = show/hide · lock button = lock\nIn edit mode, items on visible, unlocked layers can be edited"));
    m_summaryLabel->setText(text);
}