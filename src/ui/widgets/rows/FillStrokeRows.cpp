#include "ui/widgets/rows/FillStrokeRows.h"

#include "items/properties/ItemStyleVisuals.h"
#include "ui/icons/IconFactory.h"
#include "ui/widgets/RowMetrics.h"
#include "ui/widgets/RowText.h"

#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton* MakeRowIcon(IconId id, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemRowIcon"));
    const int glyph = RowMetrics::GlyphSize();
    button->setIcon(IconFactory::Make(id, QColor(139, 144, 156), glyph));
    button->setIconSize(QSize(glyph, glyph));
    // 边长跟输入框等高：既不把行撑高，也不会比属性窗里的输入框高（§D13）。
    button->setFixedSize(RowMetrics::IconButtonSize());
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QToolButton* MakeSwatch(const QColor& color, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemColorSwatch"));
    button->setFixedSize(RowMetrics::IconButtonSize());
    button->setCheckable(true);
    button->setToolTip(color.name(QColor::HexRgb).toUpper());
    button->setStyleSheet(QStringLiteral("QToolButton { border: 1px solid #CBD5E1; border-radius: 3px; "
                                         "background: %1; } QToolButton:hover { border-color: #2563EB; } "
                                         "QToolButton:checked { border: 2px solid #1D4ED8; }")
                              .arg(color.name(QColor::HexArgb)));
    button->setProperty("swatchColor", color);
    return button;
}

const QStringList& PresetColors()
{
    static const QStringList PRESET_COLORS = {
        QStringLiteral("#1E293B"), QStringLiteral("#EF4444"), QStringLiteral("#F97316"), QStringLiteral("#EAB308"),
        QStringLiteral("#22C55E"), QStringLiteral("#14B8A6"), QStringLiteral("#3B82F6"), QStringLiteral("#8B5CF6")};
    return PRESET_COLORS;
}

QHBoxLayout* MakeRowLayout(QWidget* row)
{
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    return layout;
}

} // namespace


FillStyleRow::FillStyleRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemFillStyleRow"));
    RowMetrics::ApplyRow(this);
    auto* layout = MakeRowLayout(this);
    layout->setSpacing(0);

    QToolButton* icon = MakeRowIcon(IconId::PALETTE, this);
    icon->setToolTip(RowText("Fill style"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    // 11 个按钮排一行：按钮最大 28×28、字形 20，行宽 = 图标 25 + 11×28 = 333。
    // 面板更窄时按可用宽度等比收小（不小于 20），始终排在一行、且不超过其它属性行的宽度。
    const int fillStyleButton = 28;
    const int fillStyleGlyph = 20;
    m_buttonEdge = fillStyleButton;
    for (const FillStyle style : AllFillStyles()) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::FillStyleIcon(style, fillStyleGlyph));
        button->setIconSize(QSize(fillStyleGlyph, fillStyleGlyph));
        button->setFixedSize(fillStyleButton, fillStyleButton);
        button->setToolTip(FillStyles::Name(style));
        button->setProperty("fillStyle", static_cast<int>(style));
        connect(button, &QToolButton::clicked, this, [this, style]() {
            if (!m_refreshing) {
                emit fillStyleChosen(style);
            }
        });
        m_buttons.append(button);
        layout->addWidget(button, 0, Qt::AlignVCenter);
    }
    layout->addStretch(1);
}

void FillStyleRow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_buttons.isEmpty()) {
        return;
    }
    const int iconWidth = RowMetrics::IconButtonSize().width();
    const int available = width() - iconWidth;
    if (available <= 0) {
        return;
    }
    const int edge = qBound(20, available / m_buttons.size(), 28);
    ApplyButtonSize(edge, qBound(14, edge - 8, 20));
}

void FillStyleRow::ApplyButtonSize(int edge, int glyph)
{
    if (edge <= 0 || glyph <= 0 || edge == m_buttonEdge) {
        return;
    }
    m_buttonEdge = edge;
    for (QToolButton* button : m_buttons) {
        const FillStyle style = static_cast<FillStyle>(button->property("fillStyle").toInt());
        button->setIcon(ItemStyleVisuals::FillStyleIcon(style, glyph));
        button->setIconSize(QSize(glyph, glyph));
        button->setFixedSize(edge, edge);
    }
}

void FillStyleRow::SetTargets(const QList<FillAttributes*>& targets)
{
    m_targets = targets;
    Refresh();
}

void FillStyleRow::Refresh()
{
    m_refreshing = true;
    const bool hasTargets = !m_targets.isEmpty();
    const FillStyle current = hasTargets ? m_targets.first()->fillStyle() : FillStyle::NONE;
    for (QToolButton* button : m_buttons) {
        button->setEnabled(hasTargets);
        button->setChecked(hasTargets && static_cast<FillStyle>(button->property("fillStyle").toInt()) == current);
    }
    m_refreshing = false;
}

FillColorRow::FillColorRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemFillColorRow"));
    RowMetrics::ApplyRow(this);
    auto* layout = MakeRowLayout(this);

    QToolButton* icon = MakeRowIcon(IconId::PALETTE, this);
    icon->setToolTip(RowText("Fill"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    for (const QString& hex : PresetColors()) {
        const QColor color(hex);
        QToolButton* swatch = MakeSwatch(color, this);
        connect(swatch, &QToolButton::clicked, this, [this, color]() {
            if (!m_refreshing) {
                emit fillColorChosen(color);
            }
        });
        m_swatches.append(swatch);
        layout->addWidget(swatch, 0, Qt::AlignVCenter);
    }

    m_customButton = new QToolButton(this);
    m_customButton->setObjectName(QStringLiteral("ItemColorButton"));
    m_customButton->setFixedWidth(RowMetrics::RowHeight());
    m_customButton->setIconSize(QSize(RowMetrics::GlyphSize(), RowMetrics::GlyphSize()));
    m_customButton->setToolTip(RowText("Fill color"));
    connect(m_customButton, &QToolButton::clicked, this, [this]() {
        const QColor chosen =
            QColorDialog::getColor(m_fallback, this, RowText("Fill color"), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            emit fillColorChosen(chosen);
        }
    });
    layout->addWidget(m_customButton, 0, Qt::AlignVCenter);
    layout->addStretch(1);
}

void FillColorRow::SetTargets(const QList<FillAttributes*>& targets)
{
    m_targets = targets;
    Refresh();
}

void FillColorRow::Refresh()
{
    m_refreshing = true;
    const bool hasTargets = !m_targets.isEmpty();
    m_fallback = hasTargets ? m_targets.first()->FillColor() : QColor(30, 41, 59);
    for (QToolButton* swatch : m_swatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setEnabled(hasTargets);
        swatch->setChecked(hasTargets && color.rgb() == m_fallback.rgb());
    }
    if (m_customButton) {
        m_customButton->setEnabled(hasTargets);
        m_customButton->setIcon(ItemStyleVisuals::FillColorIcon(m_fallback, RowMetrics::GlyphSize()));
        m_customButton->setToolTip(m_fallback.name(QColor::HexRgb).toUpper());
    }
    m_refreshing = false;
}

FillImageRow::FillImageRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemFillImageRow"));
    RowMetrics::ApplyRow(this);
    auto* layout = MakeRowLayout(this);

    QToolButton* icon = MakeRowIcon(IconId::IMAGE, this);
    icon->setToolTip(RowText("Fill image"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    // §D24：样式表会给按钮和下拉框各自的 min-height，只按 RowMetrics 钉高度仍会差一截。
    // 这里三者都建好、polish 完，再取「三者的 sizeHint + 输入框高」的最大值统一定高。
    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName(QStringLiteral("ItemFillImageMode"));
    m_modeCombo->addItem(RowText("Stretch"), static_cast<int>(ImageFill::Mode::STRETCH));
    m_modeCombo->addItem(RowText("Fit"), static_cast<int>(ImageFill::Mode::FIT));
    m_modeCombo->addItem(RowText("Tile"), static_cast<int>(ImageFill::Mode::TILE));
    m_modeCombo->setToolTip(RowText("Fill image mode"));
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_refreshing || index < 0) {
            return;
        }
        emit fillImageModeChosen(m_modeCombo->itemData(index).toInt());
    });
    layout->addWidget(m_modeCombo, 0);

    m_chooseButton = new QToolButton(this);
    m_chooseButton->setObjectName(QStringLiteral("ItemFillImageButton"));
    m_chooseButton->setIcon(IconFactory::Make(IconId::IMAGE, QColor(58, 64, 80), 16));
    m_chooseButton->setIconSize(QSize(16, 16));
    m_chooseButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_chooseButton->setText(RowText("Choose fill image"));
    m_chooseButton->setToolTip(RowText("Choose an image to fill the selected items"));
    connect(m_chooseButton, &QToolButton::clicked, this, &FillImageRow::chooseFillImageClicked);
    layout->insertWidget(1, m_chooseButton, 0, Qt::AlignVCenter);

    m_clearButton = new QToolButton(this);
    m_clearButton->setObjectName(QStringLiteral("ItemClearFillImageButton"));
    m_clearButton->setIcon(IconFactory::Make(IconId::DELETE, QColor(58, 64, 80), 16));
    m_clearButton->setIconSize(QSize(16, 16));
    m_clearButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_clearButton->setText(RowText("Clear"));
    m_clearButton->setToolTip(RowText("Clear fill image"));
    connect(m_clearButton, &QToolButton::clicked, this, &FillImageRow::clearFillImageClicked);
    layout->addWidget(m_clearButton, 0, Qt::AlignVCenter);
    layout->addStretch(1);

    const QList<QWidget*> fillImageFields{m_chooseButton, m_modeCombo, m_clearButton};

    for (QWidget* widget : fillImageFields) {
        widget->ensurePolished();
    }
    const int fieldHeight = qMax(RowMetrics::FieldHeight(),
                                 qMax(m_modeCombo->sizeHint().height(),
                                      qMax(m_chooseButton->sizeHint().height(), m_clearButton->sizeHint().height())));

    for (QWidget* widget : fillImageFields) {
        widget->setFixedHeight(fieldHeight);
    }
}

void FillImageRow::SetTargets(const QList<FillAttributes*>& targets)
{
    m_targets = targets;
    Refresh();
}

void FillImageRow::Refresh()
{
    m_refreshing = true;
    const bool hasTargets = !m_targets.isEmpty();
    const bool hasImage = hasTargets && m_targets.first()->HasFillImage();
    m_chooseButton->setEnabled(hasTargets);
    m_chooseButton->setText(RowText(hasImage ? "Change fill image" : "Choose fill image"));
    m_modeCombo->setEnabled(hasImage);
    m_clearButton->setEnabled(hasImage);
    const ImageFill::Mode mode = hasTargets ? m_targets.first()->FillImageMode() : ImageFill::Mode::STRETCH;
    const int index = m_modeCombo->findData(static_cast<int>(mode));
    m_modeCombo->setCurrentIndex(index >= 0 ? index : 0);
    m_refreshing = false;
}

StrokeStyleRow::StrokeStyleRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemBorderStyleRow"));
    RowMetrics::ApplyRow(this);
    auto* layout = MakeRowLayout(this);

    QToolButton* icon = MakeRowIcon(IconId::BRUSH, this);
    icon->setToolTip(RowText("Border style"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    const QList<Qt::PenStyle> styles = ItemStyleVisuals::BorderStyles();
    for (const Qt::PenStyle style : styles) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("ItemBorderStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::BorderStyleIcon(style, RowMetrics::GlyphSize()));
        button->setIconSize(QSize(RowMetrics::GlyphSize(), RowMetrics::GlyphSize()));
        button->setFixedSize(RowMetrics::IconButtonSize());
        button->setToolTip(ItemStyleVisuals::BorderStyleName(style));
        button->setProperty("penStyle", static_cast<int>(style));
        connect(button, &QToolButton::clicked, this, [this, style]() {
            if (!m_refreshing) {
                emit borderStyleChosen(style);
            }
        });
        m_buttons.append(button);
        layout->addWidget(button, 0, Qt::AlignVCenter);
    }
    layout->addStretch(1);
}

void StrokeStyleRow::SetTargets(const QList<StrokeAttributes*>& targets)
{
    m_targets = targets;
    Refresh();
}

void StrokeStyleRow::Refresh()
{
    m_refreshing = true;
    const bool hasTargets = !m_targets.isEmpty();
    const Qt::PenStyle current = hasTargets ? m_targets.first()->StrokeStyle() : Qt::SolidLine;
    for (QToolButton* button : m_buttons) {
        button->setEnabled(hasTargets);
        button->setChecked(hasTargets && static_cast<Qt::PenStyle>(button->property("penStyle").toInt()) == current);
    }
    m_refreshing = false;
}

StrokeColorRow::StrokeColorRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemBorderColorRow"));
    RowMetrics::ApplyRow(this);
    auto* layout = MakeRowLayout(this);

    QToolButton* icon = MakeRowIcon(IconId::BRUSH, this);
    icon->setToolTip(RowText("Border"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    for (const QString& hex : PresetColors()) {
        const QColor color(hex);
        QToolButton* swatch = MakeSwatch(color, this);
        connect(swatch, &QToolButton::clicked, this, [this, color]() {
            if (!m_refreshing) {
                emit strokeColorChosen(color);
            }
        });
        m_swatches.append(swatch);
        layout->addWidget(swatch, 0, Qt::AlignVCenter);
    }

    m_customButton = new QToolButton(this);
    m_customButton->setObjectName(QStringLiteral("ItemColorButton"));
    m_customButton->setFixedWidth(RowMetrics::RowHeight());
    m_customButton->setIconSize(QSize(RowMetrics::GlyphSize(), RowMetrics::GlyphSize()));
    m_customButton->setToolTip(RowText("Border color"));
    connect(m_customButton, &QToolButton::clicked, this, [this]() {
        const QColor chosen =
            QColorDialog::getColor(m_fallback, this, RowText("Border color"), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            emit strokeColorChosen(chosen);
        }
    });
    layout->addWidget(m_customButton, 0, Qt::AlignVCenter);
    layout->addStretch(1);
}

void StrokeColorRow::SetTargets(const QList<StrokeAttributes*>& targets)
{
    m_targets = targets;
    Refresh();
}

void StrokeColorRow::Refresh()
{
    m_refreshing = true;
    const bool hasTargets = !m_targets.isEmpty();
    m_fallback = hasTargets ? m_targets.first()->StrokeColor() : QColor(30, 41, 59);
    for (QToolButton* swatch : m_swatches) {
        const QColor color = swatch->property("swatchColor").value<QColor>();
        swatch->setEnabled(hasTargets);
        swatch->setChecked(hasTargets && color.rgb() == m_fallback.rgb());
    }
    if (m_customButton) {
        m_customButton->setEnabled(hasTargets);
        m_customButton->setIcon(ItemStyleVisuals::BorderColorIcon(m_fallback, RowMetrics::GlyphSize()));
        m_customButton->setToolTip(m_fallback.name(QColor::HexRgb).toUpper());
    }
    m_refreshing = false;
}

StrokeWidthRow::StrokeWidthRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemBorderWidthRow"));
    auto* layout = MakeRowLayout(this);
    layout->setSpacing(4);

    QToolButton* icon = MakeRowIcon(IconId::BRUSH, this);
    icon->setObjectName(QStringLiteral("ItemBorderWidthIcon"));
    icon->setToolTip(RowText("Border width"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setObjectName(QStringLiteral("ItemBorderWidthSpin"));
    m_widthSpin->setRange(1, 64);
    m_widthSpin->setSingleStep(1);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_widthSpin->setToolTip(RowText("Border width"));
    m_widthSpin->setKeyboardTracking(false);
    connect(m_widthSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (!m_refreshing) {
            emit borderWidthChosen(value);
        }
    });
    layout->addWidget(m_widthSpin, 0, Qt::AlignVCenter);
    layout->addStretch(1);
}

void StrokeWidthRow::SetTargets(const QList<StrokeAttributes*>& targets)
{
    m_targets = targets;
    Refresh();
}

void StrokeWidthRow::Refresh()
{
    m_refreshing = true;
    const bool hasTargets = !m_targets.isEmpty();
    m_widthSpin->setEnabled(hasTargets);
    const qreal width = hasTargets ? m_targets.first()->StrokeWidth() : 4.0;
    m_widthSpin->setValue(qRound(qMax<qreal>(1.0, width)));
    m_refreshing = false;
}
