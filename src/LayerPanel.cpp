#include "LayerPanel.h"

#include "BoardItem.h"
#include "BoardScene.h"
#include "IconFactory.h"

#include <algorithm>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
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

    auto* caption = new QLabel(QStringLiteral("上层在前 · 可多选"), this);
    caption->setObjectName(QStringLiteral("LayerCaption"));
    layout->addWidget(caption);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("LayerList"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_list, 1);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(5);
    m_addButton = makeLayerButton(this, QStringLiteral("+"), QStringLiteral("新建图层（放在最上层）"));
    m_removeButton = makeLayerButton(this, QStringLiteral("−"), QStringLiteral("删除选中的空图层（可多选）"));
    m_upButton = makeLayerButton(this, QStringLiteral("↑"), QStringLiteral("当前图层上移一层"));
    m_downButton = makeLayerButton(this, QStringLiteral("↓"), QStringLiteral("当前图层下移一层"));
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
    visibleBox->setToolTip(QStringLiteral("勾选显示该图层，取消勾选隐藏该图层"));
    visibleBox->setChecked(layer.visible);

    auto* nameLabel = new QLabel(row);
    nameLabel->setObjectName(QStringLiteral("LayerRowLabel"));
    QString text = (index == BoardScene::BackgroundLayerId)
        ? QStringLiteral("背景    （第 0 层）")
        : QStringLiteral("%1    %2 个图元").arg(layer.name).arg(itemCount);
    if (!layer.visible)
        text += QStringLiteral("  · 隐藏");
    if (layer.locked)
        text += QStringLiteral("  · 锁定");
    nameLabel->setText(text);
    if (index == BoardScene::BackgroundLayerId)
        nameLabel->setStyleSheet(QStringLiteral("color: rgb(120, 128, 140);"));
    else if (layer.locked)
        nameLabel->setStyleSheet(QStringLiteral("color: rgb(96, 104, 120);"));
    else if (!layer.visible)
        nameLabel->setStyleSheet(QStringLiteral("color: rgb(150, 157, 169);"));

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
        lockButton->setToolTip(QStringLiteral("锁定图层：锁定后该图层上的图元不可编辑"));
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
        row->setToolTip(QStringLiteral("第 %1 层 · %2 个图元\n%3%4\n编辑模式下，显示且未锁定图层上的图元都可以编辑")
                            .arg(i)
                            .arg(count)
                            .arg(layer.visible ? QStringLiteral("当前显示")
                                               : QStringLiteral("当前隐藏"),
                                 layer.locked ? QStringLiteral(" · 锁定") : QString()));
        m_list->setItemWidget(row, createLayerRow(layer, i, count));
    }

    auto* backgroundRow = new QListWidgetItem(m_list);
    backgroundRow->setData(Qt::UserRole, BoardScene::BackgroundLayerId);
    backgroundRow->setFlags(Qt::ItemIsEnabled);
    backgroundRow->setSizeHint(QSize(0, 30));
    backgroundRow->setToolTip(QStringLiteral("背景始终是最低层，不能直接放置图元\n勾选控制背景显示/隐藏"));
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

    m_list->verticalScrollBar()->setValue(scroll);
    m_refreshing = false;

    updateSummary();
    updateButtons();
}

void LayerPanel::activateCurrentLayer()
{
    if (!m_scene || m_refreshing || m_syncingScene)
        return;
    const int id = currentLayerId();
    if (id > BoardScene::BackgroundLayerId && m_scene->hasLayer(id))
        m_scene->setActiveLayer(id);
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

    updateButtons();
    updateSummary();
}

void LayerPanel::setLayerVisible(int layerId, bool visible)
{
    if (!m_scene || m_refreshing)
        return;

    const QString layerName = (layerId == BoardScene::BackgroundLayerId)
        ? QStringLiteral("背景层")
        : m_scene->layer(layerId).name;
    const int oldActive = m_scene->activeLayerId();

    if (!m_scene->setLayerVisible(layerId, visible)) {
        emit statusMessage(QStringLiteral("无法隐藏最后一个可见图层，请先新建图层。"));
        scheduleRefresh();
        return;
    }

    if (layerId == BoardScene::BackgroundLayerId) {
        emit statusMessage(visible ? QStringLiteral("已显示背景层")
                                   : QStringLiteral("已隐藏背景层"));
    } else if (!visible && oldActive == layerId
               && m_scene->activeLayerId() != oldActive) {
        emit statusMessage(QStringLiteral("已隐藏「%1」，当前图层自动切换到「%2」")
                               .arg(layerName, m_scene->layer(m_scene->activeLayerId()).name));
    } else {
        emit statusMessage(visible ? QStringLiteral("已显示图层「%1」").arg(layerName)
                                   : QStringLiteral("已隐藏图层「%1」").arg(layerName));
    }
    scheduleRefresh();
}

void LayerPanel::setLayerLocked(int layerId, bool locked)
{
    if (!m_scene || m_refreshing)
        return;

    const QString layerName = m_scene->layer(layerId).name;
    if (!m_scene->setLayerLocked(layerId, locked)) {
        emit statusMessage(QStringLiteral("背景层不支持锁定。"));
        scheduleRefresh();
        return;
    }

    emit statusMessage(locked ? QStringLiteral("已锁定图层「%1」，该层图元将不可编辑").arg(layerName)
                              : QStringLiteral("已解锁图层「%1」").arg(layerName));
    scheduleRefresh();
}

void LayerPanel::addLayer()
{
    if (!m_scene)
        return;
    m_scene->addLayerWithUndo();
    emit statusMessage(QStringLiteral("已新建图层"));
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
                this, QStringLiteral("删除图层"),
                QStringLiteral("图层「%1」上还有图元，请先移走或删除这些图元。")
                    .arg(m_scene->layer(id).name));
            return;
        }
    }
    if (m_scene->userLayerCount() - ids.size() < 1) {
        QMessageBox::information(this, QStringLiteral("删除图层"),
                                 QStringLiteral("至少需要保留一个用户图层。"));
        return;
    }

    for (int id : ids)
        m_scene->removeLayerWithUndo(id, nullptr);
    emit statusMessage(ids.size() > 1 ? QStringLiteral("已删除 %1 个图层").arg(ids.size())
                                      : QStringLiteral("已删除图层"));
    scheduleRefresh();
}

void LayerPanel::moveLayer(int delta)
{
    if (!m_scene)
        return;
    const int id = m_scene->activeLayerId();
    if (m_scene->moveLayerWithUndo(id, delta)) {
        emit statusMessage(delta > 0 ? QStringLiteral("图层已上移")
                                     : QStringLiteral("图层已下移"));
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
        text = QStringLiteral("已选中 %1 个图层（图元可多选）\n").arg(ids.size());
    } else {
        const BoardLayer active = m_scene->layer(m_scene->activeLayerId());
        text = QStringLiteral("当前：%1\n").arg(active.name);
    }
    text += QStringLiteral("勾选框＝显示/隐藏 · 锁形按钮＝锁定\n编辑模式下，显示且未锁定的图层上图元均可编辑");
    m_summaryLabel->setText(text);
}