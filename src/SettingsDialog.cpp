#include "I18n.h"

#include "SettingsDialog.h"

#include "IconFactory.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(uiText(QStringLiteral("Settings")));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(600);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto* title = new QLabel(uiText(QStringLiteral("Board settings")), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(title);

    auto* hint = new QLabel(uiText(QStringLiteral("These settings become the defaults for new brushes and canvases and are saved automatically.")), this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    rootLayout->addWidget(hint);

    auto* languageGroup = new QGroupBox(uiText(QStringLiteral("Interface language")), this);
    auto* languageForm = new QFormLayout(languageGroup);
    languageForm->setContentsMargins(16, 18, 16, 16);
    languageForm->setSpacing(10);
    m_languageCombo = new QComboBox(languageGroup);
    m_languageCombo->addItem(uiText(QStringLiteral("Follow system")), static_cast<int>(UiLanguage::System));
    m_languageCombo->addItem(uiText(QStringLiteral("Simplified Chinese")), static_cast<int>(UiLanguage::Chinese));
    m_languageCombo->addItem(QStringLiteral("English"), static_cast<int>(UiLanguage::English));
    m_languageCombo->setCurrentIndex(m_languageCombo->findData(static_cast<int>(uiLanguageSetting())));
    languageForm->addRow(uiText(QStringLiteral("Language")), m_languageCombo);
    languageForm->addRow(QString(), new QLabel(uiText(QStringLiteral("The interface updates immediately and this choice is remembered.")), languageGroup));
    rootLayout->addWidget(languageGroup);

    auto* brushGroup = new QGroupBox(uiText(QStringLiteral("Brush defaults")), this);
    auto* brushForm = new QFormLayout(brushGroup);
    brushForm->setContentsMargins(16, 18, 16, 16);
    brushForm->setSpacing(10);

    m_brushColorButton = new QPushButton(brushGroup);
    m_brushColorButton->setIconSize(QSize(18, 18));
    brushForm->addRow(uiText(QStringLiteral("Default color")), m_brushColorButton);

    m_brushWidthSpin = new QSpinBox(brushGroup);
    m_brushWidthSpin->setRange(1, 64);
    m_brushWidthSpin->setSuffix(QStringLiteral(" px"));
    m_brushWidthSpin->setValue(m_settings.brushWidth);
    m_brushWidthSpin->setFixedHeight(30);
    brushForm->addRow(uiText(QStringLiteral("Default width")), m_brushWidthSpin);

    rootLayout->addWidget(brushGroup);

    auto* canvasGroup = new QGroupBox(uiText(QStringLiteral("Canvas")), this);
    auto* canvasForm = new QFormLayout(canvasGroup);
    canvasForm->setContentsMargins(16, 18, 16, 16);
    canvasForm->setSpacing(10);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->setSpacing(8);
    m_canvasWidthSpin = new QSpinBox(canvasGroup);
    m_canvasWidthSpin->setRange(200, 12000);
    m_canvasWidthSpin->setValue(static_cast<int>(m_settings.canvasSize.width()));
    m_canvasWidthSpin->setSuffix(QStringLiteral(" px"));
    m_canvasWidthSpin->setFixedHeight(30);
    auto* multiply = new QLabel(QStringLiteral("×"), canvasGroup);
    m_canvasHeightSpin = new QSpinBox(canvasGroup);
    m_canvasHeightSpin->setRange(200, 12000);
    m_canvasHeightSpin->setValue(static_cast<int>(m_settings.canvasSize.height()));
    m_canvasHeightSpin->setSuffix(QStringLiteral(" px"));
    m_canvasHeightSpin->setFixedHeight(30);
    sizeRow->addWidget(m_canvasWidthSpin, 1);
    sizeRow->addWidget(multiply);
    sizeRow->addWidget(m_canvasHeightSpin, 1);
    canvasForm->addRow(uiText(QStringLiteral("Canvas size")), sizeRow);

    m_gridCheck = new QCheckBox(uiText(QStringLiteral("Show helper grid")), canvasGroup);
    m_gridCheck->setChecked(m_settings.showGrid);
    canvasForm->addRow(QString(), m_gridCheck);

    m_canvasColorButton = new QPushButton(canvasGroup);
    m_canvasColorButton->setIconSize(QSize(18, 18));
    canvasForm->addRow(uiText(QStringLiteral("Canvas surround color")), m_canvasColorButton);

    auto* canvasHint = new QLabel(uiText(QStringLiteral("Changing the canvas size does not move existing items; undo restores it.")), canvasGroup);
    canvasHint->setObjectName(QStringLiteral("DialogHint"));
    canvasHint->setWordWrap(true);
    canvasForm->addRow(QString(), canvasHint);

    rootLayout->addWidget(canvasGroup);

    auto* storageGroup = new QGroupBox(uiText(QStringLiteral("Canvas storage")), this);
    auto* storageLayout = new QHBoxLayout(storageGroup);
    storageLayout->setContentsMargins(16, 18, 16, 16);
    storageLayout->setSpacing(10);
    const QString canvasesDir = m_settings.canvasesDirectory.isEmpty()
        ? AppSettings::defaultCanvasesDir()
        : m_settings.canvasesDirectory;
    auto* pathEdit = new QLineEdit(QDir::toNativeSeparators(canvasesDir), storageGroup);
    pathEdit->setReadOnly(true);
    pathEdit->setCursorPosition(0);
    pathEdit->setToolTip(QDir::toNativeSeparators(canvasesDir));
    auto* openButton = new QPushButton(uiText(QStringLiteral("Open folder")), storageGroup);
    storageLayout->addWidget(pathEdit, 1);
    storageLayout->addWidget(openButton);

    rootLayout->addWidget(storageGroup);
    rootLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::Reset,
                                         this);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(QStringLiteral("Save")));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(QStringLiteral("Cancel")));
    buttons->button(QDialogButtonBox::Reset)->setText(uiText(QStringLiteral("Restore defaults")));
    rootLayout->addWidget(buttons);

    refreshColorButton(m_brushColorButton, m_settings.brushColor);
    refreshColorButton(m_canvasColorButton, m_settings.canvasColor);

    connect(m_brushColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = m_settings.brushColor;
        if (pickColor(color, uiText(QStringLiteral("Choose the default brush color")), true))
            setBrushColor(color);
    });
    connect(m_canvasColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = m_settings.canvasColor;
        if (pickColor(color, uiText(QStringLiteral("Choose canvas surround color")), false))
            setCanvasColor(color);
    });
    connect(openButton, &QPushButton::clicked, this, &SettingsDialog::openCanvasesFolder);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_settings.brushWidth = m_brushWidthSpin->value();
        m_settings.canvasSize = QSizeF(m_canvasWidthSpin->value(), m_canvasHeightSpin->value());
        m_settings.showGrid = m_gridCheck->isChecked();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &SettingsDialog::resetToDefaults);
}

void SettingsDialog::refreshColorButton(QPushButton* button, const QColor& color)
{
    button->setIcon(makeColorIcon(color, 18));
    const QString text = color.alpha() < 255 ? color.name(QColor::HexArgb).toUpper()
                                             : color.name(QColor::HexRgb).toUpper();
    button->setText(text);
}

void SettingsDialog::setBrushColor(const QColor& color)
{
    if (!color.isValid())
        return;
    m_settings.brushColor = color;
    refreshColorButton(m_brushColorButton, m_settings.brushColor);
}

void SettingsDialog::setCanvasColor(const QColor& color)
{
    if (!color.isValid())
        return;
    m_settings.canvasColor = color;
    refreshColorButton(m_canvasColorButton, m_settings.canvasColor);
}

bool SettingsDialog::pickColor(QColor& target, const QString& title, bool allowAlpha)
{
    // 使用 Qt 自绘颜色窗，避免 Windows 原生颜色对话框在部分会话/显卡驱动下
    // 进入嵌套模态循环后触发 0xc0000005（打开设置窗后点颜色时崩溃）。
    QColorDialog::ColorDialogOptions options = QColorDialog::DontUseNativeDialog;
    if (allowAlpha)
        options |= QColorDialog::ShowAlphaChannel;
    const QColor color = QColorDialog::getColor(target, this, title, options);
    if (!color.isValid())
        return false;
    target = color;
    return true;
}

void SettingsDialog::resetToDefaults()
{
    const AppSettings defaults;
    if (m_languageCombo)
        m_languageCombo->setCurrentIndex(m_languageCombo->findData(static_cast<int>(UiLanguage::System)));
    m_brushWidthSpin->setValue(defaults.brushWidth);
    m_canvasWidthSpin->setValue(static_cast<int>(defaults.canvasSize.width()));
    m_canvasHeightSpin->setValue(static_cast<int>(defaults.canvasSize.height()));
    m_gridCheck->setChecked(defaults.showGrid);
    m_settings.brushColor = defaults.brushColor;
    m_settings.canvasColor = defaults.canvasColor;
    refreshColorButton(m_brushColorButton, defaults.brushColor);
    refreshColorButton(m_canvasColorButton, defaults.canvasColor);
}

UiLanguage SettingsDialog::language() const
{
    if (!m_languageCombo)
        return uiLanguageSetting();
    return static_cast<UiLanguage>(m_languageCombo->currentData().toInt());
}

void SettingsDialog::openCanvasesFolder()
{
    const QString dir = m_settings.canvasesDirectory.isEmpty()
        ? AppSettings::defaultCanvasesDir()
        : m_settings.canvasesDirectory;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
