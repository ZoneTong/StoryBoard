#include "I18n.h"

#include "TextDialog.h"

#include "IconFactory.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QVBoxLayout>

TextDialog::TextDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(uiText(QStringLiteral("Text")));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(560);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    m_heading = new QLabel(uiText(QStringLiteral("Add text item")), this);
    m_heading->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(m_heading);

    auto* hint = new QLabel(
        uiText(QStringLiteral("Text becomes a separate item on the current layer. It can be moved and resized, and supports undo and canvas saving.")),
        this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    hint->setWordWrap(true);
    rootLayout->addWidget(hint);

    auto* group = new QGroupBox(uiText(QStringLiteral("Text and style")), this);
    auto* form = new QFormLayout(group);
    form->setContentsMargins(16, 18, 16, 16);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_edit = new QPlainTextEdit(group);
    m_edit->setObjectName(QStringLiteral("TextInput"));
    m_edit->setPlaceholderText(uiText(QStringLiteral("Enter text; press Enter for a new line…")));
    m_edit->setMinimumHeight(130);
    m_edit->setTabChangesFocus(true);
    form->addRow(uiText(QStringLiteral("Text content")), m_edit);

    m_fontCombo = new QFontComboBox(group);
    m_fontCombo->setObjectName(QStringLiteral("TextFontCombo"));
    m_fontCombo->setMinimumHeight(30);
    form->addRow(uiText(QStringLiteral("Font")), m_fontCombo);

    m_sizeSpin = new QSpinBox(group);
    m_sizeSpin->setObjectName(QStringLiteral("TextFontSize"));
    m_sizeSpin->setRange(8, 200);
    m_sizeSpin->setValue(32);
    m_sizeSpin->setSuffix(QStringLiteral(" pt"));
    m_sizeSpin->setFixedHeight(30);
    form->addRow(uiText(QStringLiteral("Size")), m_sizeSpin);

    auto* styleRow = new QHBoxLayout();
    styleRow->setSpacing(12);
    m_boldCheck = new QCheckBox(uiText(QStringLiteral("Bold")), group);
    m_italicCheck = new QCheckBox(uiText(QStringLiteral("Italic")), group);
    styleRow->addWidget(m_boldCheck);
    styleRow->addWidget(m_italicCheck);
    styleRow->addStretch(1);
    form->addRow(uiText(QStringLiteral("Style")), styleRow);

    m_colorButton = new QPushButton(group);
    m_colorButton->setObjectName(QStringLiteral("TextColorButton"));
    m_colorButton->setIconSize(QSize(18, 18));
    form->addRow(uiText(QStringLiteral("Color")), m_colorButton);

    rootLayout->addWidget(group);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(uiText(QStringLiteral("OK")));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(uiText(QStringLiteral("Cancel")));
    rootLayout->addWidget(m_buttons);

    QFont initialFont = QApplication::font();
    initialFont.setPointSize(32);
    m_fontCombo->setCurrentFont(initialFont);
    m_sizeSpin->setValue(qRound(initialFont.pointSizeF()));
    refreshColorButton();

    connect(m_edit, &QPlainTextEdit::textChanged, this, &TextDialog::updateAcceptState);
    connect(m_colorButton, &QPushButton::clicked, this, &TextDialog::chooseColor);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& font) {
        if (font.pointSizeF() > 0.0)
            m_sizeSpin->setValue(qRound(font.pointSizeF()));
    });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateAcceptState();
    m_edit->setFocus();
}

void TextDialog::setHeading(const QString& heading)
{
    if (m_heading)
        m_heading->setText(heading);
    setWindowTitle(heading);
}

void TextDialog::setTextValue(const QString& text)
{
    m_edit->setPlainText(text);
    m_edit->moveCursor(QTextCursor::End);
}

void TextDialog::setTextFont(const QFont& font)
{
    QFont effective = font;
    if (effective.pointSizeF() <= 0.0)
        effective.setPointSize(32);
    m_fontCombo->setCurrentFont(effective);
    m_sizeSpin->setValue(qBound(8, qRound(effective.pointSizeF()), 200));
    m_boldCheck->setChecked(effective.bold());
    m_italicCheck->setChecked(effective.italic());
}

void TextDialog::setTextColor(const QColor& color)
{
    if (!color.isValid())
        return;
    m_color = color;
    refreshColorButton();
}

QString TextDialog::textValue() const
{
    return m_edit->toPlainText();
}

QFont TextDialog::textFont() const
{
    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_sizeSpin->value());
    font.setBold(m_boldCheck->isChecked());
    font.setItalic(m_italicCheck->isChecked());
    return font;
}

void TextDialog::refreshColorButton()
{
    if (!m_colorButton)
        return;
    m_colorButton->setIcon(makeColorIcon(m_color, 18));
    const QString text = m_color.alpha() < 255 ? m_color.name(QColor::HexArgb).toUpper()
                                                : m_color.name(QColor::HexRgb).toUpper();
    m_colorButton->setText(text);
}

void TextDialog::updateAcceptState()
{
    if (!m_buttons)
        return;
    QPushButton* ok = m_buttons->button(QDialogButtonBox::Ok);
    if (ok)
        ok->setEnabled(!m_edit->toPlainText().trimmed().isEmpty());
}

void TextDialog::chooseColor()
{
    const QColor color = QColorDialog::getColor(
        m_color, this, uiText(QStringLiteral("Choose text color")), QColorDialog::ShowAlphaChannel);
    if (!color.isValid())
        return;
    m_color = color;
    refreshColorButton();
}
