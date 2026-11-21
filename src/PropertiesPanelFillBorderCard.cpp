#include "PropertiesPanelFillBorderCard.h"

#include "IconFactory.h"
#include "ItemStyleVisuals.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"
#include "PropertiesPanel.h"
#include "ShapeItem.h"

#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

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

PropertiesPanelFillBorderCard::PropertiesPanelFillBorderCard(QWidget* parent) : QWidget(parent)
{
    auto* fillRow = new QWidget(this);
    auto* fillLayout = new QHBoxLayout(fillRow);
    fillLayout->setContentsMargins(0, 0, 0, 0);
    fillLayout->setSpacing(2);

    QToolButton* fillIcon = MakeRowIcon(IconId::PALETTE, fillRow);
    fillIcon->setToolTip(PropertiesPanel::tr("Fill style"));
    fillLayout->addWidget(fillIcon);

    const QList<FillStyle> styles = AllFillStyles();
    for (const FillStyle style : styles) {
        auto* button = new QToolButton(fillRow);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::FillStyleIcon(style, ICON_SIZE));
        button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
        button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        button->setToolTip(FillStyleName(style));
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

    QToolButton* borderIcon = MakeRowIcon(IconId::BRUSH, styleRow);
    borderIcon->setToolTip(PropertiesPanel::tr("Border style"));
    styleLayout->addWidget(borderIcon);

    const QList<Qt::PenStyle> penStyles = ItemStyleVisuals::BorderStyles();
    for (int index = 0; index < penStyles.size(); ++index) {
        auto* button = new QToolButton(styleRow);
        button->setObjectName(QStringLiteral("ItemBorderStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::BorderStyleIcon(penStyles.at(index), ICON_SIZE));
        button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
        button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        button->setToolTip(ItemStyleVisuals::BorderStyleName(penStyles.at(index)));
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
    QToolButton* widthIcon = MakeRowIcon(IconId::BRUSH, widthRow);
    widthIcon->setObjectName(QStringLiteral("ItemBorderWidthIcon"));
    widthIcon->setToolTip(PropertiesPanel::tr("Border width"));
    widthLayout->addWidget(widthIcon);
    m_borderWidthSpin = new QSpinBox(widthRow);
    m_borderWidthSpin->setObjectName(QStringLiteral("ItemBorderWidthSpin"));
    m_borderWidthSpin->setRange(1, 64);
    m_borderWidthSpin->setSingleStep(1);
    m_borderWidthSpin->setSuffix(QStringLiteral(" px"));
    m_borderWidthSpin->setToolTip(PropertiesPanel::tr("Border width"));
    // 关闭键盘跟踪：输入过程不触发 valueChanged，回车或失焦才提交。
    m_borderWidthSpin->setKeyboardTracking(false);
    connect(m_borderWidthSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit borderWidthChosen(value);
        }
    });
    widthLayout->addWidget(m_borderWidthSpin);
    widthLayout->addStretch(1);
    m_borderWidthRow = widthRow;
    m_borderWidthRow->setObjectName(QStringLiteral("ItemBorderWidthRow"));
}

void PropertiesPanelFillBorderCard::Refresh(const QList<ShapeItem*>& shapes, const QList<LineItemAbstract*>& strokes,
                                            const QList<ShapeItem*>& composites)
{
    m_refreshing = true;

    const bool hasFillTargets = !shapes.isEmpty() || !composites.isEmpty();
    FillStyle currentFill = FillStyle::NONE;
    if (!shapes.isEmpty()) {
        currentFill = shapes.first()->fillStyle();
    } else if (!composites.isEmpty()) {
        currentFill = composites.first()->fillStyle();
    }
    for (QToolButton* button : m_fillStyleButtons) {
        const FillStyle style = static_cast<FillStyle>(button->property("fillStyle").toInt());
        button->setChecked(hasFillTargets && style == currentFill);
    }

    Qt::PenStyle currentStyle = Qt::SolidLine;
    if (!shapes.isEmpty()) {
        currentStyle = shapes.first()->StrokeStyle();
    } else if (!strokes.isEmpty()) {
        currentStyle = strokes.first()->strokePattern();
    } else if (!composites.isEmpty()) {
        currentStyle = composites.first()->StrokeStyle();
    }
    const QList<Qt::PenStyle> penStyles = ItemStyleVisuals::BorderStyles();
    const bool hasStrokeTargets = !shapes.isEmpty() || !strokes.isEmpty() || !composites.isEmpty();
    for (int index = 0; index < m_borderStyleButtons.size(); ++index) {
        m_borderStyleButtons.at(index)->setChecked(hasStrokeTargets && penStyles.at(index) == currentStyle);
    }
    if (m_borderWidthSpin) {
        m_borderWidthSpin->setEnabled(hasStrokeTargets);
        qreal width = 4.0;
        if (!shapes.isEmpty()) {
            width = shapes.first()->StrokeWidth();
        } else if (!strokes.isEmpty()) {
            width = strokes.first()->StrokeWidth();
        } else if (!composites.isEmpty()) {
            width = composites.first()->StrokeWidth();
        }
        m_borderWidthSpin->setValue(qRound(qMax<qreal>(1.0, width)));
    }

    m_refreshing = false;
}
