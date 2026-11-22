#include "ui/panels/properties/PropertiesPanelArrangeCard.h"

#include "ui/icons/IconFactory.h"
#include "ui/panels/properties/PropertiesPanel.h"

#include <QHBoxLayout>
#include <QToolButton>

namespace {

constexpr int ICON_SIZE = 20;
constexpr int SMALL_ICON_SIZE = 20;
constexpr int BUTTON_SIZE = 32;

QToolButton* MakeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    button->setIcon(MakeIcon(id, QColor(139, 144, 156), SMALL_ICON_SIZE));
    button->setIconSize(QSize(SMALL_ICON_SIZE, SMALL_ICON_SIZE));
    button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
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

    QToolButton* icon = MakeRowIcon(IconId::ORDER_UP, this);
    icon->setToolTip(PropertiesPanel::tr("Layer and height") + QLatin1Char('\n') +
                     PropertiesPanel::tr("Priority: layer order first, then height within the same layer"));
    row->addWidget(icon);

    m_layerUpButton =
        MakeButton(IconId::LAYER_UP, PropertiesPanel::tr("Layer +1"), QStringLiteral("ItemLayerUpButton"), 1, false);
    m_layerDownButton = MakeButton(IconId::LAYER_DOWN, PropertiesPanel::tr("Layer -1"),
                                   QStringLiteral("ItemLayerDownButton"), -1, false);
    m_heightUpButton =
        MakeButton(IconId::ORDER_UP, PropertiesPanel::tr("Height +1"), QStringLiteral("ItemHeightUpButton"), 1, true);
    m_heightDownButton = MakeButton(IconId::ORDER_DOWN, PropertiesPanel::tr("Height -1"),
                                    QStringLiteral("ItemHeightDownButton"), -1, true);
    row->addWidget(m_layerUpButton);
    row->addWidget(m_layerDownButton);
    row->addWidget(m_heightUpButton);
    row->addWidget(m_heightDownButton);
    row->addStretch(1);
}

QToolButton* PropertiesPanelArrangeCard::MakeButton(IconId id, const QString& toolTip, const QString& objectName,
                                                    int delta, bool height)
{
    auto* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIcon(MakeIcon(id, QColor(75, 85, 99), ICON_SIZE));
    button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
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

void PropertiesPanelArrangeCard::Refresh(bool enabled)
{
    for (QToolButton* button : {m_layerDownButton, m_layerUpButton, m_heightDownButton, m_heightUpButton}) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}
