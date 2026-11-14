#include "SettingsDialogCanvasPage.h"

#include "IconFactory.h"
#include "SettingsDialog.h"

#include <QColorDialog>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

SettingsDialogCanvasPage::SettingsDialogCanvasPage(const AppSettings& settings, QWidget* parent) : QWidget(parent)
{
    m_canvasColor = settings.canvasColor;

    m_group = new QGroupBox(SettingsDialog::tr("Canvas"), this);
    auto* form = new QFormLayout(m_group);
    form->setContentsMargins(12, 16, 12, 10);
    form->setSpacing(8);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(8);
    m_canvasWidthSpin = new QSpinBox(m_group);
    m_canvasWidthSpin->setRange(200, 12000);
    m_canvasWidthSpin->setValue(static_cast<int>(settings.canvasSize.width()));
    m_canvasWidthSpin->setSuffix(QStringLiteral(" px"));
    m_canvasWidthSpin->setFixedHeight(30);
    m_canvasWidthSpin->setToolTip(
        SettingsDialog::tr("Changing the canvas size does not move existing items; undo restores it."));
    auto* multiply = new QLabel(QStringLiteral("×"), m_group);
    m_canvasHeightSpin = new QSpinBox(m_group);
    m_canvasHeightSpin->setRange(200, 12000);
    m_canvasHeightSpin->setValue(static_cast<int>(settings.canvasSize.height()));
    m_canvasHeightSpin->setSuffix(QStringLiteral(" px"));
    m_canvasHeightSpin->setFixedHeight(30);
    m_canvasHeightSpin->setToolTip(
        SettingsDialog::tr("Changing the canvas size does not move existing items; undo restores it."));
    sizeRow->addWidget(m_canvasWidthSpin, 1);
    sizeRow->addWidget(multiply);
    sizeRow->addWidget(m_canvasHeightSpin, 1);
    form->addRow(SettingsDialog::tr("Canvas size"), sizeRow);

    m_canvasColorButton = new QPushButton(m_group);
    m_canvasColorButton->setIconSize(QSize(18, 18));
    refreshColorButton(m_canvasColorButton, m_canvasColor);
    connect(m_canvasColorButton, &QPushButton::clicked, this, [this]() {
        const QColor color = QColorDialog::getColor(
            m_canvasColor, this, SettingsDialog::tr("Choose canvas surround color"), QColorDialog::DontUseNativeDialog);
        if (!color.isValid()) {
            return;
        }
        m_canvasColor = color;
        refreshColorButton(m_canvasColorButton, m_canvasColor);
        emit canvasColorChosen(color);
    });
    form->addRow(SettingsDialog::tr("Canvas surround color"), m_canvasColorButton);

    const QString canvasesDir =
        settings.canvasesDirectory.isEmpty() ? AppSettings::defaultCanvasesDir() : settings.canvasesDirectory;
    auto* pathEdit = new QLineEdit(QDir::toNativeSeparators(canvasesDir), m_group);
    pathEdit->setReadOnly(true);
    pathEdit->setCursorPosition(0);
    pathEdit->setToolTip(QDir::toNativeSeparators(canvasesDir));
    auto* openButton = new QPushButton(SettingsDialog::tr("Open folder"), m_group);
    auto* storageRow = new QHBoxLayout();
    storageRow->setSpacing(8);
    storageRow->addWidget(pathEdit, 1);
    storageRow->addWidget(openButton);
    connect(openButton, &QPushButton::clicked, this, &SettingsDialogCanvasPage::openFolderRequested);
    form->addRow(SettingsDialog::tr("Canvas storage"), storageRow);
}

void SettingsDialogCanvasPage::writeTo(AppSettings& settings) const
{
    settings.canvasSize = QSizeF(m_canvasWidthSpin->value(), m_canvasHeightSpin->value());
}

void SettingsDialogCanvasPage::reset(const AppSettings& defaults)
{
    m_canvasWidthSpin->setValue(static_cast<int>(defaults.canvasSize.width()));
    m_canvasHeightSpin->setValue(static_cast<int>(defaults.canvasSize.height()));
    m_canvasColor = defaults.canvasColor;
    refreshColorButton(m_canvasColorButton, m_canvasColor);
}

void SettingsDialogCanvasPage::setCanvasColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_canvasColor = color;
    refreshColorButton(m_canvasColorButton, m_canvasColor);
}

void SettingsDialogCanvasPage::refreshColorButton(QPushButton* button, const QColor& color)
{
    button->setIcon(makeColorIcon(color, 18));
    const QString text =
        color.alpha() < 255 ? color.name(QColor::HexArgb).toUpper() : color.name(QColor::HexRgb).toUpper();
    button->setText(text);
}
