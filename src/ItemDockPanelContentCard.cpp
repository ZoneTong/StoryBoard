#include "ItemDockPanelContentCard.h"

#include "IconFactory.h"
#include "ImageItem.h"
#include "ItemDockPanel.h"
#include "ItemStyleVisuals.h"
#include "TextItem.h"

#include <QColorDialog>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace {

constexpr int kSmallIconSize = 16;
constexpr int kButtonSize = 28;
constexpr int kSwatchSize = 24;

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

QToolButton* makeSwatch(const QColor& color, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ItemTextColorSwatch"));
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

} // namespace

ItemDockPanelContentCard::ItemDockPanelContentCard(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    m_textContentRow = new QWidget(this);
    m_textContentRow->setObjectName(QStringLiteral("ItemTextContentRow"));
    auto* contentLayout = new QHBoxLayout(m_textContentRow);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);
    QToolButton* contentIcon = makeRowIcon(IconId::Text, m_textContentRow);
    contentIcon->setToolTip(ItemDockPanel::tr("Text content"));
    contentLayout->addWidget(contentIcon);
    m_textEdit = new QLineEdit(m_textContentRow);
    m_textEdit->setObjectName(QStringLiteral("ItemTextContentEdit"));
    m_textEdit->setPlaceholderText(ItemDockPanel::tr("Text content"));
    m_textEdit->setToolTip(ItemDockPanel::tr("Edit the selected text"));
    connect(m_textEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!m_refreshing) {
            emit textContentChosen(m_textEdit->text());
        }
    });
    contentLayout->addWidget(m_textEdit, 1);
    outer->addWidget(m_textContentRow);

    m_textFontRow = new QWidget(this);
    m_textFontRow->setObjectName(QStringLiteral("ItemTextFontRow"));
    auto* fontLayout = new QHBoxLayout(m_textFontRow);
    fontLayout->setContentsMargins(0, 0, 0, 0);
    fontLayout->setSpacing(3);
    QToolButton* fontIcon = makeRowIcon(IconId::Text, m_textFontRow);
    fontIcon->setToolTip(ItemDockPanel::tr("Font"));
    fontLayout->addWidget(fontIcon);
    m_fontCombo = new QFontComboBox(m_textFontRow);
    m_fontCombo->setObjectName(QStringLiteral("ItemTextFontCombo"));
    m_fontCombo->setToolTip(ItemDockPanel::tr("Font"));
    m_fontCombo->setMinimumWidth(88);
    fontLayout->addWidget(m_fontCombo, 1);
    m_fontSizeSpin = new QSpinBox(m_textFontRow);
    m_fontSizeSpin->setObjectName(QStringLiteral("ItemTextFontSizeSpin"));
    m_fontSizeSpin->setRange(6, 96);
    m_fontSizeSpin->setSuffix(QStringLiteral(" pt"));
    m_fontSizeSpin->setToolTip(ItemDockPanel::tr("Font size"));
    m_fontSizeSpin->setFixedWidth(62);
    fontLayout->addWidget(m_fontSizeSpin);
    m_boldButton = new QToolButton(m_textFontRow);
    m_boldButton->setObjectName(QStringLiteral("ItemTextBoldButton"));
    m_boldButton->setText(QStringLiteral("B"));
    m_boldButton->setCheckable(true);
    m_boldButton->setFixedSize(kButtonSize, kButtonSize);
    m_boldButton->setToolTip(ItemDockPanel::tr("Bold"));
    QFont boldFont = m_boldButton->font();
    boldFont.setBold(true);
    m_boldButton->setFont(boldFont);
    fontLayout->addWidget(m_boldButton);
    m_italicButton = new QToolButton(m_textFontRow);
    m_italicButton->setObjectName(QStringLiteral("ItemTextItalicButton"));
    m_italicButton->setText(QStringLiteral("I"));
    m_italicButton->setCheckable(true);
    m_italicButton->setFixedSize(kButtonSize, kButtonSize);
    m_italicButton->setToolTip(ItemDockPanel::tr("Italic"));
    QFont italicFont = m_italicButton->font();
    italicFont.setItalic(true);
    m_italicButton->setFont(italicFont);
    fontLayout->addWidget(m_italicButton);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this]() { emitFontChanged(); });
    connect(m_fontSizeSpin, &QSpinBox::valueChanged, this, [this]() { emitFontChanged(); });
    connect(m_boldButton, &QToolButton::toggled, this, [this]() { emitFontChanged(); });
    connect(m_italicButton, &QToolButton::toggled, this, [this]() { emitFontChanged(); });
    outer->addWidget(m_textFontRow);

    m_textColorRow = makeTextColorRow();
    outer->addWidget(m_textColorRow);

    m_imageRow = new QWidget(this);
    m_imageRow->setObjectName(QStringLiteral("ItemImageRow"));
    auto* imageLayout = new QHBoxLayout(m_imageRow);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(4);
    QToolButton* imageIcon = makeRowIcon(IconId::Image, m_imageRow);
    imageIcon->setToolTip(ItemDockPanel::tr("Image name"));
    imageLayout->addWidget(imageIcon);
    m_imageNameEdit = new QLineEdit(m_imageRow);
    m_imageNameEdit->setObjectName(QStringLiteral("ItemImageNameEdit"));
    m_imageNameEdit->setPlaceholderText(ItemDockPanel::tr("Image name"));
    m_imageNameEdit->setToolTip(ItemDockPanel::tr("Image name"));
    connect(m_imageNameEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!m_refreshing) {
            emit imageNameChosen(m_imageNameEdit->text());
        }
    });
    imageLayout->addWidget(m_imageNameEdit, 1);
    m_replaceImageButton = new QToolButton(m_imageRow);
    m_replaceImageButton->setObjectName(QStringLiteral("ItemReplaceImageButton"));
    m_replaceImageButton->setIcon(makeIcon(IconId::Image, QColor(75, 85, 99), kSmallIconSize));
    m_replaceImageButton->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    m_replaceImageButton->setFixedSize(kButtonSize, kButtonSize);
    m_replaceImageButton->setAutoRaise(true);
    m_replaceImageButton->setToolTip(ItemDockPanel::tr("Replace image…"));
    connect(m_replaceImageButton, &QToolButton::clicked, this, &ItemDockPanelContentCard::replaceImageRequested);
    imageLayout->addWidget(m_replaceImageButton);
    outer->addWidget(m_imageRow);

    hide();
}

QWidget* ItemDockPanelContentCard::makeTextColorRow()
{
    auto* row = new QWidget(this);
    row->setObjectName(QStringLiteral("ItemTextColorRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QToolButton* icon = makeRowIcon(IconId::Palette, row);
    icon->setToolTip(ItemDockPanel::tr("Text color"));
    layout->addWidget(icon);

    const QStringList presets = {QStringLiteral("#1E293B"), QStringLiteral("#EF4444"), QStringLiteral("#F97316"),
                                 QStringLiteral("#EAB308"), QStringLiteral("#22C55E"), QStringLiteral("#14B8A6"),
                                 QStringLiteral("#3B82F6"), QStringLiteral("#8B5CF6")};
    for (const QString& hex : presets) {
        const QColor color(hex);
        QToolButton* swatch = makeSwatch(color, row);
        connect(swatch, &QToolButton::clicked, this, [this, color]() {
            if (!m_refreshing) {
                emit textColorChosen(color);
            }
        });
        m_textColorSwatches.append(swatch);
        layout->addWidget(swatch);
    }

    m_textColorButton = new QToolButton(row);
    m_textColorButton->setObjectName(QStringLiteral("ItemTextColorButton"));
    m_textColorButton->setFixedSize(kSwatchSize, kSwatchSize);
    m_textColorButton->setIconSize(QSize(kSmallIconSize, kSmallIconSize));
    m_textColorButton->setIcon(ItemStyleVisuals::fillColorIcon(m_textColorFallback, kSmallIconSize));
    m_textColorButton->setToolTip(ItemDockPanel::tr("Text color"));
    connect(m_textColorButton, &QToolButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_textColorFallback, this, ItemDockPanel::tr("Text color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid() && !m_refreshing) {
            emit textColorChosen(chosen);
        }
    });
    layout->addWidget(m_textColorButton);
    layout->addStretch(1);
    return row;
}

void ItemDockPanelContentCard::refresh(const QList<TextItem*>& texts, const QList<ImageItem*>& images)
{
    m_refreshing = true;

    const bool hasText = !texts.isEmpty();
    m_textContentRow->setVisible(hasText);
    m_textFontRow->setVisible(hasText);
    m_textColorRow->setVisible(hasText);
    if (hasText) {
        TextItem* first = texts.first();
        m_textFont = first->textFont();
        if (!m_textEdit->hasFocus()) {
            m_textEdit->setText(first->text());
        }
        m_fontCombo->setCurrentFont(m_textFont);
        m_fontSizeSpin->setValue(qMax(1, qRound(m_textFont.pointSizeF())));
        m_boldButton->setChecked(m_textFont.bold());
        m_italicButton->setChecked(m_textFont.italic());
        m_textColorFallback = first->textColor();
        for (QToolButton* swatch : std::as_const(m_textColorSwatches)) {
            const QColor color = swatch->property("swatchColor").value<QColor>();
            swatch->setChecked(color.rgb() == m_textColorFallback.rgb());
        }
        if (m_textColorButton) {
            m_textColorButton->setIcon(ItemStyleVisuals::fillColorIcon(m_textColorFallback, kSmallIconSize));
            m_textColorButton->setToolTip(m_textColorFallback.name(QColor::HexRgb).toUpper());
        }
    }

    const bool hasImage = !images.isEmpty();
    m_imageRow->setVisible(hasImage);
    if (hasImage) {
        if (!m_imageNameEdit->hasFocus()) {
            m_imageNameEdit->setText(images.first()->imageName());
        }
        m_replaceImageButton->setEnabled(images.size() == 1);
    }

    setVisible(hasText || hasImage);
    m_refreshing = false;
}

QFont ItemDockPanelContentCard::composedFont() const
{
    QFont font = m_textFont;
    if (m_fontCombo) {
        font.setFamily(m_fontCombo->currentFont().family());
    }
    if (m_fontSizeSpin) {
        font.setPointSize(m_fontSizeSpin->value());
    }
    if (m_boldButton) {
        font.setBold(m_boldButton->isChecked());
    }
    if (m_italicButton) {
        font.setItalic(m_italicButton->isChecked());
    }
    return font;
}

void ItemDockPanelContentCard::emitFontChanged()
{
    if (!m_refreshing) {
        emit textFontChosen(composedFont());
    }
}
