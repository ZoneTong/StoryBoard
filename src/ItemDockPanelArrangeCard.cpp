#include "ItemDockPanelArrangeCard.h"

#include "IconFactory.h"
#include "ItemDockPanel.h"

#include <QHBoxLayout>
#include <QToolButton>

namespace {

constexpr int kIconSize = 16;
constexpr int kSmallIconSize = 14;

QToolButton* makeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    button->setIcon(makeIcon(id, QColor(139, 144, 156), kSmallIconSize));
    button->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    button->setFixedSize(kSmallIconSize + 4, kSmallIconSize + 4);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

ItemDockPanelArrangeCard::ItemDockPanelArrangeCard(QWidget* parent) : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);

    QToolButton* icon = makeRowIcon(IconId::OrderUp, this);
    icon->setToolTip(ItemDockPanel::tr("Layer and height") + QLatin1Char('\n') +
                     ItemDockPanel::tr("Priority: layer order first, then height within the same layer"));
    row->addWidget(icon);

    m_layerUpButton =
        makeButton(IconId::LayerUp, ItemDockPanel::tr("Layer +1"), QStringLiteral("ItemLayerUpButton"), 1, false);
    m_layerDownButton =
        makeButton(IconId::LayerDown, ItemDockPanel::tr("Layer -1"), QStringLiteral("ItemLayerDownButton"), -1, false);
    m_heightUpButton =
        makeButton(IconId::OrderUp, ItemDockPanel::tr("Height +1"), QStringLiteral("ItemHeightUpButton"), 1, true);
    m_heightDownButton =
        makeButton(IconId::OrderDown, ItemDockPanel::tr("Height -1"), QStringLiteral("ItemHeightDownButton"), -1, true);
    row->addWidget(m_layerUpButton);
    row->addWidget(m_layerDownButton);
    row->addWidget(m_heightUpButton);
    row->addWidget(m_heightDownButton);
    row->addStretch(1);
}

QToolButton* ItemDockPanelArrangeCard::makeButton(IconId id, const QString& toolTip, const QString& objectName,
                                                  int delta, bool height)
{
    auto* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIcon(makeIcon(id, QColor(75, 85, 99), kIconSize));
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setFixedSize(24, 24);
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    connect(button, &QToolButton::clicked, this, [this, delta, height]() {
        if (height) {
            emit heightAdjusted(delta);
        } else {
            emit layerAdjusted(delta);
        }
    });
    return button;
}

void ItemDockPanelArrangeCard::refresh(bool enabled)
{
    for (QToolButton* button : {m_layerDownButton, m_layerUpButton, m_heightDownButton, m_heightUpButton}) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}
