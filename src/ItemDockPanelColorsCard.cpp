#include "ItemDockPanelColorsCard.h"

#include "IconFactory.h"
#include "ItemDockPanel.h"
#include "ItemStyleVisuals.h"
#include "ShapeItem.h"
#include "StrokeItem.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kSmallIconSize = 16;
constexpr int kButtonSize = 28;
constexpr int kSwatchSize = 24;

QToolButton* makeSwatch(const QColor& color, QWidget* parent, const QString& objectName)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setFixedSize(kSwatchSize, kSwatchSize);
    button->setCheckable(true);
    button->setToolTip(color.name(QColor::HexRgb).toUpper());
    button->setStyleSheet(QStringLiteral("QToolButton { border: 1px solid #CBD5E1; border-radius: 3px; "
                                         "background: %1; } QToolButton:hover { border-color: #2563EB; } "
                                         "QToolButton:checked { border: 2px solid #1D4ED8; }")
                              .arg(color.name(QColor::HexArgb)));
    button->setProperty("swatchColor", color);
    return button;
}

QString colorTip(const QColor& color)
{
    return color.name(QColor::HexRgb).toUpper();
}

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

ItemDockPanelColorsCard::ItemDockPanelColorsCard(QWidget* parent) : QWidget(parent)
{
    m_borderColorRow = makeColorRow(true);
    m_borderColorRow->setObjectName(QStringLiteral("ItemBorderColorRow"));
    m_fillColorRow = makeColorRow(false);
    m_fillColorRow->setObjectName(QStringLiteral("ItemFillColorRow"));
}

QWidget* ItemDockPanelColorsCard::makeColorRow(bool stroke)
{
    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);

    QToolButton* icon = makeRowIcon(stroke ? IconId::Brush : IconId::Palette, row);
    icon->setToolTip(stroke ? ItemDockPanel::tr("Border") : ItemDockPanel::tr("Fill"));
    rowLayout->addWidget(icon);

    const QStringList presets = {QStringLiteral("#1E293B"), QStringLiteral("#EF4444"), QStringLiteral("#F97316"),
                                 QStringLiteral("#EAB308"), QStringLiteral("#22C55E"), QStringLiteral("#14B8A6"),
                                 QStringLiteral("#3B82F6"), QStringLiteral("#8B5CF6")};
    QList<QToolButton*>* swatches = stroke ? &m_strokeSwatches : &m_fillSwatches;
    for (const QString& hex : presets) {
        const QColor color(hex);
        QToolButton* swatch = makeSwatch(color, row, QStringLiteral("ItemColorSwatch"));
        connect(swatch, &QToolButton::clicked, this, [this, color, stroke]() {
            if (stroke) {
                emit strokeColorChosen(color);
            } else {
                emit fillColorChosen(color);
            }
        });
        rowLayout->addWidget(swatch);
        swatches->append(swatch);
    }

    auto* custom = new QToolButton(row);
    custom->setObjectName(QStringLiteral("ItemColorButton"));
    custom->setFixedSize(kSwatchSize, kSwatchSize);
    custom->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    custom->setIcon(stroke ? ItemStyleVisuals::borderColorIcon(m_strokeFallback, kSmallIconSize)
                           : ItemStyleVisuals::fillColorIcon(m_fillFallback, kSmallIconSize));
    custom->setToolTip(stroke ? ItemDockPanel::tr("Border color") : ItemDockPanel::tr("Fill color"));
    connect(custom, &QToolButton::clicked, this, [this, stroke]() {
        const QColor initial = stroke ? m_strokeFallback : m_fillFallback;
        const QColor chosen = QColorDialog::getColor(
            initial, this, stroke ? ItemDockPanel::tr("Border color") : ItemDockPanel::tr("Fill color"),
            QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) {
            return;
        }
        if (stroke) {
            emit strokeColorChosen(chosen);
        } else {
            emit fillColorChosen(chosen);
        }
    });
    rowLayout->addWidget(custom);

    if (stroke) {
        m_strokeColorButton = custom;
    } else {
        m_fillColorButton = custom;
    }
    rowLayout->addStretch(1);
    return row;
}

void ItemDockPanelColorsCard::refresh(const QList<ShapeItem*>& shapes, const QList<StrokeItem*>& strokes)
{
    QColor strokeFallback = QColor(QStringLiteral("#1E293B"));
    if (!shapes.isEmpty()) {
        strokeFallback = shapes.first()->strokeColor();
    } else if (!strokes.isEmpty()) {
        strokeFallback = strokes.first()->strokeColor();
    }
    m_strokeFallback = strokeFallback;
    m_fillFallback = shapes.isEmpty() ? QColor(QStringLiteral("#1E293B")) : shapes.first()->fillColor();
    for (QToolButton* swatch : m_strokeSwatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setChecked(color.rgb() == m_strokeFallback.rgb());
    }
    for (QToolButton* swatch : m_fillSwatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setChecked(color.rgb() == m_fillFallback.rgb());
    }
    if (m_strokeColorButton) {
        m_strokeColorButton->setIcon(ItemStyleVisuals::borderColorIcon(m_strokeFallback, kSmallIconSize));
        m_strokeColorButton->setToolTip(colorTip(m_strokeFallback));
    }
    if (m_fillColorButton) {
        m_fillColorButton->setIcon(ItemStyleVisuals::fillColorIcon(m_fillFallback, kSmallIconSize));
        m_fillColorButton->setToolTip(colorTip(m_fillFallback));
    }
}
