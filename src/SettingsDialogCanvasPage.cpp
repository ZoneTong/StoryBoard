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
    // 棋盘格边长固定 64px，画布宽高按格对齐；输入任意值也会在应用时吸附到 64 的整数倍。
    m_canvasWidthSpin->setSingleStep(64);
    m_canvasWidthSpin->setValue(static_cast<int>(settings.canvasSize.width()));
    m_canvasWidthSpin->setSuffix(QStringLiteral(" px"));
    m_canvasWidthSpin->setFixedHeight(30);
    m_canvasWidthSpin->setToolTip(
        SettingsDialog::tr("Changing the canvas size does not move existing items; undo restores it. "
                           "The width is snapped to a multiple of the 64 px checkerboard cell."));
    auto* multiply = new QLabel(QStringLiteral("×"), m_group);
    m_canvasHeightSpin = new QSpinBox(m_group);
    m_canvasHeightSpin->setRange(200, 12000);
    m_canvasHeightSpin->setSingleStep(64);
    m_canvasHeightSpin->setValue(static_cast<int>(settings.canvasSize.height()));
    m_canvasHeightSpin->setSuffix(QStringLiteral(" px"));
    m_canvasHeightSpin->setFixedHeight(30);
    m_canvasHeightSpin->setToolTip(
        SettingsDialog::tr("Changing the canvas size does not move existing items; undo restores it. "
                           "The height is snapped to a multiple of the 64 px checkerboard cell."));
    sizeRow->addWidget(m_canvasWidthSpin, 1);
    sizeRow->addWidget(multiply);
    sizeRow->addWidget(m_canvasHeightSpin, 1);
    form->addRow(SettingsDialog::tr("Canvas size"), sizeRow);

    m_canvasColorButton = new QPushButton(m_group);
    m_canvasColorButton->setIconSize(QSize(18, 18));
    RefreshColorButton(m_canvasColorButton, m_canvasColor);
    connect(m_canvasColorButton, &QPushButton::clicked, this, [this]() {
        const QColor color = QColorDialog::getColor(
            m_canvasColor, this, SettingsDialog::tr("Choose canvas surround color"), QColorDialog::DontUseNativeDialog);
        if (!color.isValid()) {
            return;
        }
        m_canvasColor = color;
        RefreshColorButton(m_canvasColorButton, m_canvasColor);
        emit canvasColorChosen(color);
    });
    form->addRow(SettingsDialog::tr("Canvas surround color"), m_canvasColorButton);

    const QString canvasesDir =
        settings.canvasesDirectory.isEmpty() ? AppSettings::DefaultCanvasesDir() : settings.canvasesDirectory;
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

void SettingsDialogCanvasPage::WriteTo(AppSettings& settings) const
{
    // 需求 16：画布宽高必须是棋盘格的整数倍，不接受半格；这里吸附到最近的 64 倍数。
    constexpr int CELL = 64;
    const int width = qMax(CELL, qRound(m_canvasWidthSpin->value() / static_cast<qreal>(CELL)) * CELL);
    const int height = qMax(CELL, qRound(m_canvasHeightSpin->value() / static_cast<qreal>(CELL)) * CELL);
    settings.canvasSize = QSizeF(width, height);
}

void SettingsDialogCanvasPage::Reset(const AppSettings& defaults)
{
    m_canvasWidthSpin->setValue(static_cast<int>(defaults.canvasSize.width()));
    m_canvasHeightSpin->setValue(static_cast<int>(defaults.canvasSize.height()));
    m_canvasColor = defaults.canvasColor;
    RefreshColorButton(m_canvasColorButton, m_canvasColor);
}

void SettingsDialogCanvasPage::SetCanvasColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_canvasColor = color;
    RefreshColorButton(m_canvasColorButton, m_canvasColor);
}

void SettingsDialogCanvasPage::RefreshColorButton(QPushButton* button, const QColor& color)
{
    button->setIcon(MakeColorIcon(color, 18));
    const QString text =
        color.alpha() < 255 ? color.name(QColor::HexArgb).toUpper() : color.name(QColor::HexRgb).toUpper();
    button->setText(text);
}
