#include "ui/panels/layers/LayerRow.h"

#include "app/I18n.h"
#include "scene/CanvasScene.h"
#include "ui/icons/IconFactory.h"
#include "ui/panels/layers/LayerPanel.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>

LayerRow::LayerRow(const CanvasLayer& layer, int index, int itemCount, QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("LayerRow"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(5);

    m_visibleBox = new QCheckBox(this);
    m_visibleBox->setObjectName(QStringLiteral("LayerVisibleBox"));
    m_visibleBox->setFocusPolicy(Qt::NoFocus);
    m_visibleBox->setToolTip(LayerPanel::tr("Check to show this layer, uncheck to hide it"));
    m_visibleBox->setChecked(layer.visible);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("LayerRowLabel"));
    QString text = (index == CanvasScene::BackgroundLayerId)
                       ? LayerPanel::tr("Background    (layer 0)")
                       : LayerPanel::tr("%1    %2 items").arg(layer.name).arg(itemCount);
    if (!layer.visible) {
        text += LayerPanel::tr("  · Hidden");
    }
    if (layer.locked) {
        text += LayerPanel::tr("  · Locked");
    }
    m_nameLabel->setText(text);
    // 背景层 / 锁定 / 隐藏三种弱化文字色走 QSS 的 state 属性，不再逐行写内联样式。
    if (index == CanvasScene::BackgroundLayerId) {
        m_nameLabel->setProperty("state", QStringLiteral("background"));
    } else if (layer.locked) {
        m_nameLabel->setProperty("state", QStringLiteral("locked"));
    } else if (!layer.visible) {
        m_nameLabel->setProperty("state", QStringLiteral("hidden"));
    }

    if (index != CanvasScene::BackgroundLayerId) {
        m_layerId = layer.id;
        m_nameLabel->setProperty("layerId", layer.id);
        m_nameLabel->setCursor(Qt::IBeamCursor);
        m_nameLabel->setToolTip(LayerPanel::tr("Double-click to rename layer"));
        m_nameLabel->installEventFilter(this);
    }

    layout->addWidget(m_visibleBox);
    layout->addWidget(m_nameLabel, 1);

    if (index != CanvasScene::BackgroundLayerId) {
        m_lockButton = new QToolButton(this);
        m_lockButton->setObjectName(QStringLiteral("LayerLockButton"));
        m_lockButton->setCheckable(true);
        m_lockButton->setChecked(layer.locked);
        m_lockButton->setFocusPolicy(Qt::NoFocus);
        m_lockButton->setFixedSize(22, 22);
        m_lockButton->setIcon(MakeIcon(IconId::LOCK, layer.locked ? QColor(37, 99, 235) : QColor(110, 118, 132), 18));
        m_lockButton->setIconSize(QSize(18, 18));
        m_lockButton->setToolTip(LayerPanel::tr("Lock layer: items on it cannot be edited"));
        layout->addWidget(m_lockButton);
        connect(m_lockButton, &QToolButton::toggled, this,
                [this](bool locked) { emit lockToggled(m_layerId, locked); });
    }

    connect(m_visibleBox, &QCheckBox::toggled, this,
            [this](bool visible) { emit visibilityToggled(m_layerId, visible); });
}

void LayerRow::ApplySelectedStyle(bool selected)
{
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

bool LayerRow::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::MouseButtonDblClick && watched == m_nameLabel) {
        emit renameRequested(m_layerId);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
