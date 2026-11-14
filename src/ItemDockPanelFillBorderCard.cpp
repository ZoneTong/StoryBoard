#include "ItemDockPanelFillBorderCard.h"

#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "ItemStyleVisuals.h"
#include "ShapeItem.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

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

ItemDockPanelFillBorderCard::ItemDockPanelFillBorderCard(QWidget* parent) : QWidget(parent)
{
    auto* fillRow = new QWidget(this);
    auto* fillLayout = new QHBoxLayout(fillRow);
    fillLayout->setContentsMargins(0, 0, 0, 0);
    fillLayout->setSpacing(2);

    QToolButton* fillIcon = makeRowIcon(IconId::Palette, fillRow);
    fillIcon->setToolTip(ItemDockPanel::tr("Fill style"));
    fillLayout->addWidget(fillIcon);

    const QList<FillStyle> styles = allFillStyles();
    for (const FillStyle style : styles) {
        auto* button = new QToolButton(fillRow);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::fillStyleIcon(style, kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(fillStyleName(style));
        button->setProperty("fillStyle", static_cast<int>(style));
        connect(button, &QToolButton::clicked, this, [this, style]() { emit fillStyleChosen(style); });
        m_fillStyleButtons.append(button);
        fillLayout->addWidget(button);
    }
    fillLayout->addStretch(1);
    m_fillStyleRow = fillRow;
    m_fillStyleRow->setObjectName(QStringLiteral("ItemFillStyleRow"));

    auto* styleRow = new QWidget(this);
    auto* styleLayout = new QHBoxLayout(styleRow);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(2);

    QToolButton* borderIcon = makeRowIcon(IconId::Brush, styleRow);
    borderIcon->setToolTip(ItemDockPanel::tr("Border style"));
    styleLayout->addWidget(borderIcon);

    const QList<Qt::PenStyle> penStyles = ItemStyleVisuals::borderStyles();
    for (int index = 0; index < penStyles.size(); ++index) {
        auto* button = new QToolButton(styleRow);
        button->setObjectName(QStringLiteral("ItemBorderStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::borderStyleIcon(penStyles.at(index), kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(ItemStyleVisuals::borderStyleName(penStyles.at(index)));
        const Qt::PenStyle style = penStyles.at(index);
        connect(button, &QToolButton::clicked, this, [this, style]() { emit borderStyleChosen(style); });
        m_borderStyleButtons.append(button);
        styleLayout->addWidget(button);
    }
    styleLayout->addStretch(1);
    m_borderStyleRow = styleRow;
    m_borderStyleRow->setObjectName(QStringLiteral("ItemBorderStyleRow"));

    auto* widthRow = new QWidget(this);
    auto* widthLayout = new QHBoxLayout(widthRow);
    widthLayout->setContentsMargins(0, 0, 0, 0);
    widthLayout->setSpacing(4);
    m_borderWidthSpin = new QDoubleSpinBox(widthRow);
    m_borderWidthSpin->setObjectName(QStringLiteral("ItemValueSpin"));
    m_borderWidthSpin->setRange(0.5, 48.0);
    m_borderWidthSpin->setSingleStep(0.5);
    m_borderWidthSpin->setDecimals(1);
    m_borderWidthSpin->setSuffix(QStringLiteral(" px"));
    m_borderWidthSpin->setToolTip(ItemDockPanel::tr("Border width"));
    connect(m_borderWidthSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_refreshing) {
            emit borderWidthChosen(value);
        }
    });
    widthLayout->addWidget(m_borderWidthSpin);
    widthLayout->addStretch(1);
    m_borderWidthRow = widthRow;
    m_borderWidthRow->setObjectName(QStringLiteral("ItemBorderWidthRow"));
}

void ItemDockPanelFillBorderCard::refresh(const QList<ShapeItem*>& shapes)
{
    m_refreshing = true;

    const FillStyle currentFill = shapes.isEmpty() ? FillStyle::None : shapes.first()->fillStyle();
    for (QToolButton* button : m_fillStyleButtons) {
        const FillStyle style = static_cast<FillStyle>(button->property("fillStyle").toInt());
        button->setChecked(!shapes.isEmpty() && style == currentFill);
    }

    const Qt::PenStyle currentStyle = shapes.isEmpty() ? Qt::SolidLine : shapes.first()->strokeStyle();
    const QList<Qt::PenStyle> penStyles = ItemStyleVisuals::borderStyles();
    for (int index = 0; index < m_borderStyleButtons.size(); ++index) {
        m_borderStyleButtons.at(index)->setChecked(!shapes.isEmpty() && penStyles.at(index) == currentStyle);
    }
    if (m_borderWidthSpin) {
        m_borderWidthSpin->setEnabled(!shapes.isEmpty());
        m_borderWidthSpin->setValue(shapes.isEmpty() ? 4.0 : shapes.first()->strokeWidth());
    }

    m_refreshing = false;
}
