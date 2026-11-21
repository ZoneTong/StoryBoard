#include "PropertiesPanelColorsCard.h"

#include "IconFactory.h"
#include "ItemStyleVisuals.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"
#include "PropertiesPanel.h"
#include "ShapeItem.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int SMALL_ICON_SIZE = 20;
constexpr int BUTTON_SIZE = 32;
constexpr int SWATCH_SIZE = 26;

QToolButton* makeSwatch(const QColor& color, QWidget* parent, const QString& objectName)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setFixedSize(SWATCH_SIZE, SWATCH_SIZE);
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
    button->setIcon(MakeIcon(id, QColor(139, 144, 156), SMALL_ICON_SIZE));
    button->setIconSize(QSize(SMALL_ICON_SIZE, SMALL_ICON_SIZE));
    button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

PropertiesPanelColorsCard::PropertiesPanelColorsCard(QWidget* parent) : QWidget(parent)
{
    m_borderColorRow = MakeColorRow(true);
    m_borderColorRow->setObjectName(QStringLiteral("ItemBorderColorRow"));
    m_fillColorRow = MakeColorRow(false);
    m_fillColorRow->setObjectName(QStringLiteral("ItemFillColorRow"));
}

QWidget* PropertiesPanelColorsCard::MakeColorRow(bool stroke)
{
    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);

    QToolButton* icon = makeRowIcon(stroke ? IconId::BRUSH : IconId::PALETTE, row);
    icon->setToolTip(stroke ? PropertiesPanel::tr("Border") : PropertiesPanel::tr("Fill"));
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
    custom->setFixedSize(SWATCH_SIZE, SWATCH_SIZE);
    custom->setIconSize(QSize(SMALL_ICON_SIZE, SMALL_ICON_SIZE));
    custom->setIcon(stroke ? ItemStyleVisuals::BorderColorIcon(m_strokeFallback, SMALL_ICON_SIZE)
                           : ItemStyleVisuals::FillColorIcon(m_fillFallback, SMALL_ICON_SIZE));
    custom->setToolTip(stroke ? PropertiesPanel::tr("Border color") : PropertiesPanel::tr("Fill color"));
    connect(custom, &QToolButton::clicked, this, [this, stroke]() {
        const QColor initial = stroke ? m_strokeFallback : m_fillFallback;
        const QColor chosen = QColorDialog::getColor(
            initial, this, stroke ? PropertiesPanel::tr("Border color") : PropertiesPanel::tr("Fill color"),
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

void PropertiesPanelColorsCard::Refresh(const QList<ShapeItem*>& shapes, const QList<LineItemAbstract*>& strokes,
                                        const QList<ShapeItem*>& composites)
{
    QColor strokeFallback = QColor(QStringLiteral("#1E293B"));
    if (!shapes.isEmpty()) {
        strokeFallback = shapes.first()->StrokeColor();
    } else if (!strokes.isEmpty()) {
        strokeFallback = strokes.first()->StrokeColor();
    } else if (!composites.isEmpty()) {
        strokeFallback = composites.first()->StrokeColor();
    }
    m_strokeFallback = strokeFallback;
    QColor fillFallback = QColor(QStringLiteral("#1E293B"));
    if (!shapes.isEmpty()) {
        fillFallback = shapes.first()->FillColor();
    } else if (!composites.isEmpty()) {
        fillFallback = composites.first()->FillColor();
    }
    m_fillFallback = fillFallback;
    for (QToolButton* swatch : m_strokeSwatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setChecked(color.rgb() == m_strokeFallback.rgb());
    }
    for (QToolButton* swatch : m_fillSwatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setChecked(color.rgb() == m_fillFallback.rgb());
    }
    if (m_strokeColorButton) {
        m_strokeColorButton->setIcon(ItemStyleVisuals::BorderColorIcon(m_strokeFallback, SMALL_ICON_SIZE));
        m_strokeColorButton->setToolTip(colorTip(m_strokeFallback));
    }
    if (m_fillColorButton) {
        m_fillColorButton->setIcon(ItemStyleVisuals::FillColorIcon(m_fillFallback, SMALL_ICON_SIZE));
        m_fillColorButton->setToolTip(colorTip(m_fillFallback));
    }
}
