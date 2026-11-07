#include "LayerPanel.h"

#include "BoardScene.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
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
    setMinimumWidth(218);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(7);

    auto* caption = new QLabel(QStringLiteral("上层在前"), this);
    caption->setObjectName(QStringLiteral("LayerCaption"));
    layout->addWidget(caption);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("LayerList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_list, 1);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(5);
    m_addButton = makeLayerButton(this, QStringLiteral("+"), QStringLiteral("新建图层（放在最上层）"));
    m_removeButton = makeLayerButton(this, QStringLiteral("−"), QStringLiteral("删除当前空图层"));
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

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) {
        activateCurrentLayer();
    });
    connect(m_list, &QListWidget::itemChanged, this, &LayerPanel::toggleLayerVisibility);
    connect(m_addButton, &QToolButton::clicked, this, &LayerPanel::addLayer);
    connect(m_removeButton, &QToolButton::clicked, this, &LayerPanel::removeLayer);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    connect(m_downButton, &QToolButton::clicked, this, [this]() { moveLayer(-1); });

    connect(m_scene, &BoardScene::layersChanged, this, &LayerPanel::refresh);
    connect(m_scene, &BoardScene::activeLayerChanged, this, &LayerPanel::syncActiveLayer);
    connect(m_scene, &BoardScene::contentChanged, this, &LayerPanel::refresh);

    refresh();
}

int LayerPanel::selectedLayerId() const
{
    const QListWidgetItem* current = m_list->currentItem();
    return current ? current->data(Qt::UserRole).toInt() : 0;
}

void LayerPanel::refresh()
{
    if (!m_scene || m_refreshing)
        return;

    m_refreshing = true;
    const QSignalBlocker blocker(m_list);
    const int scroll = m_list->verticalScrollBar()->value();
    m_list->clear();

    // Top of the list is the highest layer. Background is always the lowest
    // row and cannot receive items.
    for (int i = m_scene->layers().size() - 1; i >= 1; --i) {
        const BoardLayer layer = m_scene->layers().at(i);
        const int count = m_scene->layerItemCount(layer.id);
        QString text = QStringLiteral("%1    %2 个图元").arg(layer.name).arg(count);
        if (!layer.visible)
            text += QStringLiteral("  [隐藏]");
        auto* row = new QListWidgetItem(text, m_list);
        row->setData(Qt::UserRole, layer.id);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        if (!layer.visible)
            row->setForeground(QColor(150, 157, 169));
        row->setToolTip(QStringLiteral("第 %1 层 · %2 个图元\n%3\n选中图层会自动显示，且只有该层图元可以编辑")
                            .arg(i)
                            .arg(count)
                            .arg(layer.visible ? QStringLiteral("当前显示")
                                               : QStringLiteral("当前隐藏")));
        if (layer.id == m_scene->activeLayerId())
            m_list->setCurrentItem(row);
    }

    auto* backgroundRow = new QListWidgetItem(
        m_scene->isLayerVisible(BoardScene::BackgroundLayerId)
            ? QStringLiteral("背景    （第 0 层）")
            : QStringLiteral("背景    （第 0 层，隐藏）"),
        m_list);
    backgroundRow->setData(Qt::UserRole, BoardScene::BackgroundLayerId);
    backgroundRow->setFlags((backgroundRow->flags() & ~Qt::ItemIsSelectable)
                            | Qt::ItemIsUserCheckable);
    backgroundRow->setCheckState(m_scene->isLayerVisible(BoardScene::BackgroundLayerId)
                                     ? Qt::Checked
                                     : Qt::Unchecked);
    backgroundRow->setForeground(QColor(120, 128, 140));
    backgroundRow->setToolTip(QStringLiteral("背景始终是最低层，不能直接放置图元\n勾选控制背景显示/隐藏"));

    if (!m_list->currentItem()) {
        for (int row = 0; row < m_list->count(); ++row) {
            if (m_list->item(row)->data(Qt::UserRole).toInt() == m_scene->activeLayerId()) {
                m_list->setCurrentRow(row);
                break;
            }
        }
    }
    m_list->verticalScrollBar()->setValue(scroll);
    m_refreshing = false;

    const BoardLayer active = m_scene->layer(m_scene->activeLayerId());
    m_summaryLabel->setText(
        QStringLiteral("当前：%1 · 仅当前图层可编辑\n勾选图层控制显示/隐藏")
            .arg(active.name));
    updateButtons();
}

void LayerPanel::activateCurrentLayer()
{
    if (!m_scene || m_refreshing)
        return;
    const int id = selectedLayerId();
    if (id > BoardScene::BackgroundLayerId && m_scene->hasLayer(id))
        m_scene->setActiveLayer(id);
    updateButtons();
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
    const BoardLayer active = m_scene->layer(m_scene->activeLayerId());
    m_summaryLabel->setText(
        QStringLiteral("当前：%1 · 仅当前图层可编辑\n勾选图层控制显示/隐藏")
            .arg(active.name));
    m_refreshing = false;
    updateButtons();
}

void LayerPanel::toggleLayerVisibility(QListWidgetItem* item)
{
    if (!m_scene || m_refreshing || !item)
        return;

    const int layerId = item->data(Qt::UserRole).toInt();
    const bool visible = item->checkState() == Qt::Checked;
    const QString layerName = m_scene->layer(layerId).name;
    const int oldActive = m_scene->activeLayerId();

    if (!m_scene->setLayerVisible(layerId, visible)) {
        emit statusMessage(QStringLiteral("无法隐藏最后一个可编辑图层，请先新建图层。"));
        refresh();
        return;
    }

    if (layerId == BoardScene::BackgroundLayerId) {
        emit statusMessage(visible ? QStringLiteral("已显示背景层")
                                   : QStringLiteral("已隐藏背景层"));
    } else if (!visible && oldActive == layerId
               && m_scene->activeLayerId() != oldActive) {
        emit statusMessage(
            QStringLiteral("已隐藏「%1」，当前图层自动切换到「%2」")
                .arg(layerName, m_scene->layer(m_scene->activeLayerId()).name));
    } else if (visible) {
        emit statusMessage(QStringLiteral("已显示图层「%1」").arg(layerName));
    } else {
        emit statusMessage(QStringLiteral("已隐藏图层「%1」").arg(layerName));
    }
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
    const int id = selectedLayerId();
    QString error;
    if (!m_scene->removeLayerWithUndo(id, &error)) {
        QMessageBox::information(this, QStringLiteral("删除图层"), error);
        return;
    }
    emit statusMessage(QStringLiteral("已删除图层"));
}

void LayerPanel::moveLayer(int delta)
{
    if (!m_scene)
        return;
    const int id = selectedLayerId();
    if (m_scene->moveLayerWithUndo(id, delta)) {
        emit statusMessage(delta > 0 ? QStringLiteral("图层已上移")
                                     : QStringLiteral("图层已下移"));
    }
}

void LayerPanel::updateButtons()
{
    if (!m_scene)
        return;

    const int id = selectedLayerId();
    const int index = m_scene->layerIndex(id);
    const bool userLayer = index > 0;
    m_addButton->setEnabled(true);
    m_removeButton->setEnabled(userLayer && m_scene->userLayerCount() > 1
                               && m_scene->layerItemCount(id) == 0);
    m_upButton->setEnabled(userLayer && index < m_scene->layerCount() - 1);
    m_downButton->setEnabled(userLayer && index > 1);
}
