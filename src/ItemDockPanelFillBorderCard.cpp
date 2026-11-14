#include "ItemDockPanelFillBorderCard.h"

#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "ShapeItem.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kIconSize = 16;
constexpr int kSmallIconSize = 14;

// 填充样式按钮的缩略图：None 画叉、Solid 纯色、纹理复用 ShapeItem 的纹理中心线。
QIcon makeFillStyleIcon(FillStyle style, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rect(3.0, 3.0, logicalSize - 6.0, logicalSize - 6.0);
    painter.setPen(QPen(QColor(120, 128, 145), 1.2));
    if (style == FillStyle::None) {
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
        painter.drawLine(rect.topLeft(), rect.bottomRight());
        painter.drawLine(rect.topRight(), rect.bottomLeft());
        painter.end();
        return QIcon(pixmap);
    }
    if (style == FillStyle::Solid) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(58, 64, 80));
        painter.drawRect(rect);
        painter.end();
        return QIcon(pixmap);
    }
    painter.setBrush(QColor(248, 249, 252));
    painter.drawRect(rect);
    painter.setPen(QPen(QColor(58, 64, 80), 1.0, Qt::SolidLine, Qt::FlatCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(fillPatternCenterLines(style, rect));
    painter.end();
    return QIcon(pixmap);
}

// 边框样式按钮的缩略图：按 4 种 PenStyle 画一小段线。
QIcon makeBorderStyleIcon(Qt::PenStyle style, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(58, 64, 80), 1.8, style, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(4.0, logicalSize / 2.0), QPointF(logicalSize - 4.0, logicalSize / 2.0));
    painter.end();
    return QIcon(pixmap);
}

// 边框样式按钮的提示文案。
QString borderStyleName(Qt::PenStyle style)
{
    switch (style) {
    case Qt::SolidLine:
        return ItemDockPanel::tr("Solid");
    case Qt::DashLine:
        return ItemDockPanel::tr("Dashed");
    case Qt::DotLine:
        return ItemDockPanel::tr("Dotted");
    case Qt::DashDotLine:
        return ItemDockPanel::tr("Dash-dot");
    default:
        return ItemDockPanel::tr("Solid");
    }
}

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
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    auto* fillRow = new QWidget(this);
    auto* fillLayout = new QHBoxLayout(fillRow);
    fillLayout->setContentsMargins(0, 0, 0, 0);
    fillLayout->setSpacing(2);

    QToolButton* fillIcon = makeRowIcon(IconId::Image, fillRow);
    fillIcon->setToolTip(ItemDockPanel::tr("Fill style"));
    fillLayout->addWidget(fillIcon);

    const QList<FillStyle> styles = allFillStyles();
    for (const FillStyle style : styles) {
        auto* button = new QToolButton(fillRow);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeFillStyleIcon(style, kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(fillStyleName(style));
        button->setProperty("fillStyle", static_cast<int>(style));
        connect(button, &QToolButton::clicked, this, [this, style]() { emit fillStyleChosen(style); });
        m_fillStyleButtons.append(button);
        fillLayout->addWidget(button);
    }
    fillLayout->addStretch(1);
    outer->addWidget(fillRow);

    auto* styleRow = new QWidget(this);
    auto* styleLayout = new QHBoxLayout(styleRow);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(2);

    QToolButton* borderIcon = makeRowIcon(IconId::Edit, styleRow);
    borderIcon->setToolTip(ItemDockPanel::tr("Border style"));
    styleLayout->addWidget(borderIcon);

    const QList<Qt::PenStyle> penStyles = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
    for (int index = 0; index < penStyles.size(); ++index) {
        auto* button = new QToolButton(styleRow);
        button->setObjectName(QStringLiteral("ItemBorderStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(makeBorderStyleIcon(penStyles.at(index), kIconSize));
        button->setIconSize(QSize(kIconSize, kIconSize));
        button->setFixedSize(24, 24);
        button->setToolTip(borderStyleName(penStyles.at(index)));
        const Qt::PenStyle style = penStyles.at(index);
        connect(button, &QToolButton::clicked, this, [this, style]() { emit borderStyleChosen(style); });
        m_borderStyleButtons.append(button);
        styleLayout->addWidget(button);
    }
    styleLayout->addStretch(1);
    outer->addWidget(styleRow);

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
    m_borderWidthSpin->setToolTip(ItemDockPanel::tr("Width"));
    connect(m_borderWidthSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_refreshing) {
            emit borderWidthChosen(value);
        }
    });
    widthLayout->addWidget(m_borderWidthSpin);
    widthLayout->addStretch(1);
    outer->addWidget(widthRow);
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
    const QList<Qt::PenStyle> penStyles = {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
    for (int index = 0; index < m_borderStyleButtons.size(); ++index) {
        m_borderStyleButtons.at(index)->setChecked(!shapes.isEmpty() && penStyles.at(index) == currentStyle);
    }
    if (m_borderWidthSpin) {
        m_borderWidthSpin->setEnabled(!shapes.isEmpty());
        m_borderWidthSpin->setValue(shapes.isEmpty() ? 4.0 : shapes.first()->strokeWidth());
    }

    m_refreshing = false;
}
