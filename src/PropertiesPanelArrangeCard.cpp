#include "PropertiesPanelArrangeCard.h"

#include "IconFactory.h"
#include "PropertiesPanel.h"

#include <QHBoxLayout>
#include <QToolButton>

namespace {

constexpr int kIconSize = 20;
constexpr int kSmallIconSize = 20;
constexpr int kButtonSize = 32;

QToolButton* makeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    button->setIcon(makeIcon(id, QColor(139, 144, 156), kSmallIconSize));
    button->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

PropertiesPanelArrangeCard::PropertiesPanelArrangeCard(QWidget* parent) : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);

    QToolButton* icon = makeRowIcon(IconId::OrderUp, this);
    icon->setToolTip(PropertiesPanel::tr("Layer and height") + QLatin1Char('\n') +
                     PropertiesPanel::tr("Priority: layer order first, then height within the same layer"));
    row->addWidget(icon);

    m_layerUpButton =
        makeButton(IconId::LayerUp, PropertiesPanel::tr("Layer +1"), QStringLiteral("ItemLayerUpButton"), 1, false);
    m_layerDownButton = makeButton(IconId::LayerDown, PropertiesPanel::tr("Layer -1"),
                                   QStringLiteral("ItemLayerDownButton"), -1, false);
    m_heightUpButton =
        makeButton(IconId::OrderUp, PropertiesPanel::tr("Height +1"), QStringLiteral("ItemHeightUpButton"), 1, true);
    m_heightDownButton = makeButton(IconId::OrderDown, PropertiesPanel::tr("Height -1"),
                                    QStringLiteral("ItemHeightDownButton"), -1, true);
    row->addWidget(m_layerUpButton);
    row->addWidget(m_layerDownButton);
    row->addWidget(m_heightUpButton);
    row->addWidget(m_heightDownButton);
    row->addStretch(1);
}

QToolButton* PropertiesPanelArrangeCard::makeButton(IconId id, const QString& toolTip, const QString& objectName,
                                                    int delta, bool height)
{
    auto* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIcon(makeIcon(id, QColor(75, 85, 99), kIconSize));
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setFixedSize(kButtonSize, kButtonSize);
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

void PropertiesPanelArrangeCard::refresh(bool enabled)
{
    for (QToolButton* button : {m_layerDownButton, m_layerUpButton, m_heightDownButton, m_heightUpButton}) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}
