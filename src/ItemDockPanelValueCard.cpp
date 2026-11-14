#include "ItemDockPanelValueCard.h"

#include "CanvasItem.h"
#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "ShapeItem.h"

#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

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

ItemDockPanelValueCard::ItemDockPanelValueCard(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    auto* opacityRow = new QWidget(this);
    auto* opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(4);

    QToolButton* opacityIcon = makeRowIcon(IconId::Opacity, opacityRow);
    opacityIcon->setToolTip(ItemDockPanel::tr("Opacity"));
    opacityLayout->addWidget(opacityIcon);

    m_opacitySpin = new QSpinBox(opacityRow);
    m_opacitySpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_opacitySpin->setRange(0, 100);
    m_opacitySpin->setSuffix(QStringLiteral(" %"));
    m_opacitySpin->setToolTip(ItemDockPanel::tr("Opacity"));
    connect(m_opacitySpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit opacityChosen(value);
        }
    });
    opacityLayout->addWidget(m_opacitySpin, 1);
    outer->addWidget(opacityRow);

    m_cornerSection = new QWidget(this);
    auto* cornerLayout = new QHBoxLayout(m_cornerSection);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->setSpacing(4);

    QToolButton* cornerIcon = makeRowIcon(IconId::Corner, m_cornerSection);
    cornerIcon->setToolTip(ItemDockPanel::tr("Corner radius"));
    cornerLayout->addWidget(cornerIcon);

    m_cornerSpin = new QSpinBox(m_cornerSection);
    m_cornerSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_cornerSpin->setRange(0, 400);
    m_cornerSpin->setSuffix(QStringLiteral(" px"));
    m_cornerSpin->setToolTip(ItemDockPanel::tr("Corner radius"));
    connect(m_cornerSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit cornerRadiusChosen(value);
        }
    });
    cornerLayout->addWidget(m_cornerSpin, 1);
    outer->addWidget(m_cornerSection);

    m_angleSection = new QWidget(this);
    auto* angleLayout = new QHBoxLayout(m_angleSection);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->setSpacing(4);

    QToolButton* angleIcon = makeRowIcon(IconId::RotateCcw, m_angleSection);
    angleIcon->setToolTip(ItemDockPanel::tr("Sweep angle"));
    angleLayout->addWidget(angleIcon);

    m_angleSpin = new QSpinBox(m_angleSection);
    m_angleSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_angleSpin->setRange(1, 359);
    m_angleSpin->setSuffix(QStringLiteral("°"));
    m_angleSpin->setToolTip(ItemDockPanel::tr("Sweep angle"));
    connect(m_angleSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit sweepAngleChosen(value);
        }
    });
    angleLayout->addWidget(m_angleSpin, 1);
    outer->addWidget(m_angleSection);

    m_radiusSection = new QWidget(this);
    auto* radiusLayout = new QHBoxLayout(m_radiusSection);
    radiusLayout->setContentsMargins(0, 0, 0, 0);
    radiusLayout->setSpacing(4);

    QToolButton* radiusIcon = makeRowIcon(IconId::Corner, m_radiusSection);
    radiusIcon->setToolTip(ItemDockPanel::tr("Shape radius"));
    radiusLayout->addWidget(radiusIcon);

    m_radiusSpin = new QSpinBox(m_radiusSection);
    m_radiusSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_radiusSpin->setRange(1, 1000);
    m_radiusSpin->setSuffix(QStringLiteral(" px"));
    m_radiusSpin->setToolTip(ItemDockPanel::tr("Shape radius"));
    connect(m_radiusSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit shapeRadiusChosen(value);
        }
    });
    radiusLayout->addWidget(m_radiusSpin, 1);
    outer->addWidget(m_radiusSection);
}

void ItemDockPanelValueCard::refresh(const QList<ShapeItem*>& shapes, const QList<CanvasItem*>& items)
{
    m_refreshing = true;

    const bool enabled = !shapes.isEmpty();
    if (m_opacitySpin) {
        m_opacitySpin->setEnabled(!items.isEmpty());
        m_opacitySpin->setValue(items.isEmpty() ? 100 : qRound(items.first()->itemOpacity() * 100.0));
    }
    if (m_cornerSpin) {
        m_cornerSpin->setEnabled(enabled);
        const qreal radius = shapes.isEmpty() ? 0.0 : qMax<qreal>(0.0, shapes.first()->cornerRadius());
        m_cornerSpin->setValue(qRound(radius));
    }
    if (m_angleSpin) {
        m_angleSpin->setEnabled(enabled);
        m_angleSpin->setValue(shapes.isEmpty() ? 120 : qRound(shapes.first()->sweepAngle()));
    }
    if (m_radiusSpin) {
        m_radiusSpin->setEnabled(enabled);
        // 自动半径（<0）显示为实际生效的短边一半，用户改一下即成为显式半径。
        qreal radius = 100.0;
        if (!shapes.isEmpty()) {
            const qreal stored = shapes.first()->shapeRadius();
            radius = stored >= 0.0 ? stored : shapes.first()->contentSize().width() / 2.0;
        }
        m_radiusSpin->setValue(qMax(1, qRound(radius)));
    }

    m_refreshing = false;
}

void ItemDockPanelValueCard::setCornerVisible(bool visible)
{
    if (m_cornerSection) {
        m_cornerSection->setVisible(visible);
    }
}

void ItemDockPanelValueCard::setArcRadiusVisible(bool visible)
{
    if (m_angleSection) {
        m_angleSection->setVisible(visible);
    }
    if (m_radiusSection) {
        m_radiusSection->setVisible(visible);
    }
}
