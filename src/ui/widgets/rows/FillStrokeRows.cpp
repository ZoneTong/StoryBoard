#include "ui/widgets/rows/FillStrokeRows.h"

#include "items/properties/ItemStyleVisuals.h"
#include "ui/icons/IconFactory.h"
#include "ui/widgets/RowMetrics.h"

#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
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

QString AttributeRowText(const char* source)
{
    // 编辑面板与属性窗共用同一批行控件，译文沿用 PropertiesPanel 上下文。
    return QCoreApplication::translate("PropertiesPanel", source);
}

void SetRowIconVisible(QWidget* row, bool visible)
{
    if (!row) {
        return;
    }
    for (const char* name : {"ItemRowIcon", "ItemBorderWidthIcon"}) {
        for (QToolButton* icon : row->findChildren<QToolButton*>(QString::fromLatin1(name))) {
            icon->setVisible(visible);
        }
    }
}

FillStyleRow::FillStyleRow(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ItemFillStyleRow"));
    RowMetrics::ApplyRow(this);
    auto* layout = MakeRowLayout(this);
    layout->setSpacing(0);

    QToolButton* icon = MakeRowIcon(IconId::PALETTE, this);
    icon->setToolTip(AttributeRowText("Fill style"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    // 11 个按钮：宽度收到 18，行总宽 = 图标 25 + 11×18 = 223，不超过图元尺寸行（≈227）；
    // 高度取统一行高，所以是窄长条而不是小方块（§D25）。
    const int fillStyleWidth = 18;
    const int fillStyleGlyph = 16;
    for (const FillStyle style : AllFillStyles()) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("ItemFillStyleButton"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIcon(ItemStyleVisuals::FillStyleIcon(style, fillStyleGlyph));
        button->setIconSize(QSize(fillStyleGlyph, fillStyleGlyph));
        button->setFixedSize(fillStyleWidth, RowMetrics::RowHeight());
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
    icon->setToolTip(AttributeRowText("Fill"));
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
    m_customButton->setToolTip(AttributeRowText("Fill color"));
    connect(m_customButton, &QToolButton::clicked, this, [this]() {
        const QColor chosen =
            QColorDialog::getColor(m_fallback, this, AttributeRowText("Fill color"), QColorDialog::ShowAlphaChannel);
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

    const int fieldHeight = RowMetrics::FieldHeight();

    QToolButton* icon = MakeRowIcon(IconId::IMAGE, this);
    icon->setToolTip(AttributeRowText("Fill image"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    m_chooseButton = new QToolButton(this);
    m_chooseButton->setObjectName(QStringLiteral("ItemFillImageButton"));
    m_chooseButton->setIcon(IconFactory::Make(IconId::IMAGE, QColor(58, 64, 80), 16));
    m_chooseButton->setIconSize(QSize(16, 16));
    m_chooseButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_chooseButton->setText(AttributeRowText("Choose fill image"));
    m_chooseButton->setToolTip(AttributeRowText("Choose an image to fill the selected items"));
    // §D24：三个控件统一到输入框高，按钮才不会比旁边的“拉伸”下拉框高一截。
    m_chooseButton->setFixedHeight(fieldHeight);
    connect(m_chooseButton, &QToolButton::clicked, this, &FillImageRow::chooseFillImageClicked);
    layout->addWidget(m_chooseButton, 0, Qt::AlignVCenter);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName(QStringLiteral("ItemFillImageMode"));
    m_modeCombo->addItem(AttributeRowText("Stretch"), static_cast<int>(ImageFill::Mode::STRETCH));
    m_modeCombo->addItem(AttributeRowText("Fit"), static_cast<int>(ImageFill::Mode::FIT));
    m_modeCombo->addItem(AttributeRowText("Tile"), static_cast<int>(ImageFill::Mode::TILE));
    m_modeCombo->setFixedHeight(fieldHeight);
    m_modeCombo->setToolTip(AttributeRowText("Fill image mode"));
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_refreshing || index < 0) {
            return;
        }
        emit fillImageModeChosen(m_modeCombo->itemData(index).toInt());
    });
    layout->addWidget(m_modeCombo, 0, Qt::AlignVCenter);

    m_clearButton = new QToolButton(this);
    m_clearButton->setObjectName(QStringLiteral("ItemClearFillImageButton"));
    m_clearButton->setIcon(IconFactory::Make(IconId::DELETE, QColor(58, 64, 80), 16));
    m_clearButton->setIconSize(QSize(16, 16));
    m_clearButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_clearButton->setText(AttributeRowText("Clear"));
    m_clearButton->setFixedHeight(fieldHeight);
    m_clearButton->setToolTip(AttributeRowText("Clear fill image"));
    connect(m_clearButton, &QToolButton::clicked, this, &FillImageRow::clearFillImageClicked);
    layout->addWidget(m_clearButton, 0, Qt::AlignVCenter);
    layout->addStretch(1);
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
    m_chooseButton->setText(AttributeRowText(hasImage ? "Change fill image" : "Choose fill image"));
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
    icon->setToolTip(AttributeRowText("Border style"));
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
    icon->setToolTip(AttributeRowText("Border"));
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
    m_customButton->setToolTip(AttributeRowText("Border color"));
    connect(m_customButton, &QToolButton::clicked, this, [this]() {
        const QColor chosen =
            QColorDialog::getColor(m_fallback, this, AttributeRowText("Border color"), QColorDialog::ShowAlphaChannel);
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
    icon->setToolTip(AttributeRowText("Border width"));
    layout->addWidget(icon, 0, Qt::AlignVCenter);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setObjectName(QStringLiteral("ItemBorderWidthSpin"));
    m_widthSpin->setRange(1, 64);
    m_widthSpin->setSingleStep(1);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_widthSpin->setToolTip(AttributeRowText("Border width"));
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
