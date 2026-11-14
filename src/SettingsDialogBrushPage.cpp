#include "SettingsDialogBrushPage.h"

#include "IconFactory.h"
#include "SettingsDialog.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>

SettingsDialogBrushPage::SettingsDialogBrushPage(const AppSettings& settings, QWidget* parent) : QWidget(parent)
{
    m_brushColor = settings.brushColor;

    m_group = new QGroupBox(SettingsDialog::tr("Brush defaults"), this);
    auto* form = new QFormLayout(m_group);
    form->setContentsMargins(12, 16, 12, 10);
    form->setSpacing(8);

    m_brushColorButton = new QPushButton(m_group);
    m_brushColorButton->setIconSize(QSize(18, 18));
    refreshColorButton(m_brushColorButton, m_brushColor);
    connect(m_brushColorButton, &QPushButton::clicked, this, [this]() {
        // 使用 Qt 自绘颜色窗，避免 Windows 原生颜色对话框在部分会话/显卡驱动下
        // 进入嵌套模态循环后触发 0xc0000005（打开设置窗后点颜色时崩溃）。
        const QColor color =
            QColorDialog::getColor(m_brushColor, this, SettingsDialog::tr("Choose the default brush color"),
                                   QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);
        if (!color.isValid()) {
            return;
        }
        m_brushColor = color;
        refreshColorButton(m_brushColorButton, m_brushColor);
        emit brushColorChosen(color);
    });
    form->addRow(SettingsDialog::tr("Default color"), m_brushColorButton);

    m_brushWidthSpin = new QSpinBox(m_group);
    m_brushWidthSpin->setRange(1, 64);
    m_brushWidthSpin->setSuffix(QStringLiteral(" px"));
    m_brushWidthSpin->setValue(settings.brushWidth);
    m_brushWidthSpin->setFixedHeight(30);
    form->addRow(SettingsDialog::tr("Default width"), m_brushWidthSpin);

    m_smoothingSpin = new QSpinBox(m_group);
    m_smoothingSpin->setObjectName(QStringLiteral("StrokeSmoothingSpin"));
    m_smoothingSpin->setRange(0, 30);
    m_smoothingSpin->setSuffix(QStringLiteral(" px"));
    m_smoothingSpin->setValue(settings.strokeSmoothing);
    m_smoothingSpin->setFixedHeight(30);
    m_smoothingSpin->setToolTip(
        SettingsDialog::tr("0 disables automatic smoothing; larger values tolerate more hand jitter and deviation."));
    form->addRow(SettingsDialog::tr("Auto-smooth tolerance"), m_smoothingSpin);
}

void SettingsDialogBrushPage::writeTo(AppSettings& settings) const
{
    settings.brushWidth = m_brushWidthSpin->value();
    settings.strokeSmoothing = m_smoothingSpin->value();
}

void SettingsDialogBrushPage::reset(const AppSettings& defaults)
{
    m_brushWidthSpin->setValue(defaults.brushWidth);
    m_smoothingSpin->setValue(defaults.strokeSmoothing);
    m_brushColor = defaults.brushColor;
    refreshColorButton(m_brushColorButton, m_brushColor);
}

void SettingsDialogBrushPage::setBrushColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_brushColor = color;
    refreshColorButton(m_brushColorButton, m_brushColor);
}

void SettingsDialogBrushPage::refreshColorButton(QPushButton* button, const QColor& color)
{
    button->setIcon(makeColorIcon(color, 18));
    const QString text =
        color.alpha() < 255 ? color.name(QColor::HexArgb).toUpper() : color.name(QColor::HexRgb).toUpper();
    button->setText(text);
}
