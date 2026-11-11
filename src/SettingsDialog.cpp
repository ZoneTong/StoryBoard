#include "I18n.h"

#include "SettingsDialog.h"

#include "IconFactory.h"
#include "LineArtPresets.h"

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
#include <QSignalBlocker>
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

    m_strokeSmoothingSpin = new QSpinBox(brushGroup);
    m_strokeSmoothingSpin->setObjectName(QStringLiteral("StrokeSmoothingSpin"));
    m_strokeSmoothingSpin->setRange(0, 30);
    m_strokeSmoothingSpin->setSuffix(QStringLiteral(" px"));
    m_strokeSmoothingSpin->setValue(m_settings.strokeSmoothing);
    m_strokeSmoothingSpin->setFixedHeight(30);
    m_strokeSmoothingSpin->setToolTip(uiText(QStringLiteral("0 disables automatic smoothing; larger values tolerate more hand jitter and deviation.")));
    brushForm->addRow(uiText(QStringLiteral("Auto-smooth tolerance")), m_strokeSmoothingSpin);

    auto* smoothingHint = new QLabel(uiText(QStringLiteral("0 disables automatic smoothing; larger values tolerate more hand jitter and deviation.")), brushGroup);
    smoothingHint->setObjectName(QStringLiteral("DialogHint"));
    smoothingHint->setWordWrap(true);
    brushForm->addRow(QString(), smoothingHint);

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

    // 图片线稿化的参数：颜色怎么并、碎块怎么并，都要能自己调，否则遇到别的图只能重编译。
    // 上面再加一个档位，一键把下面几项换成一组实测过的组合。
    auto* lineArtGroup = new QGroupBox(uiText(QStringLiteral("Image outline tracing")), this);
    auto* lineArtForm = new QFormLayout(lineArtGroup);
    lineArtForm->setContentsMargins(16, 18, 16, 16);
    lineArtForm->setSpacing(10);

    m_lineArtPresetCombo = new QComboBox(lineArtGroup);
    m_lineArtPresetCombo->setObjectName(QStringLiteral("LineArtPresetCombo"));
    m_lineArtPresetCombo->setFixedHeight(30);
    for (int i = 0; i < lineArtPresetCount(); ++i)
        m_lineArtPresetCombo->addItem(uiText(QString::fromLatin1(lineArtPreset(i).name)), i);
    m_lineArtPresetCombo->addItem(uiText(QStringLiteral("Custom")), -1);
    m_lineArtPresetCombo->setToolTip(uiText(QStringLiteral("Applies a tested parameter set to all items below; editing any value switches back to Custom.")));
    lineArtForm->addRow(uiText(QStringLiteral("Preset")), m_lineArtPresetCombo);

    m_lineArtLevelsSpin = new QSpinBox(lineArtGroup);
    m_lineArtLevelsSpin->setObjectName(QStringLiteral("LineArtLevelsSpin"));
    m_lineArtLevelsSpin->setRange(2, 12);
    m_lineArtLevelsSpin->setValue(m_settings.lineArtColorLevels);
    m_lineArtLevelsSpin->setFixedHeight(30);
    m_lineArtLevelsSpin->setToolTip(uiText(QStringLiteral("How many steps each color channel is cut into; more steps keeps subtle shading but also more fragments.")));
    lineArtForm->addRow(uiText(QStringLiteral("Color levels")), m_lineArtLevelsSpin);

    m_lineArtPaletteMergeSpin = new QSpinBox(lineArtGroup);
    m_lineArtPaletteMergeSpin->setObjectName(QStringLiteral("LineArtPaletteMergeSpin"));
    m_lineArtPaletteMergeSpin->setRange(0, 160);
    m_lineArtPaletteMergeSpin->setValue(m_settings.lineArtPaletteMerge);
    m_lineArtPaletteMergeSpin->setFixedHeight(30);
    m_lineArtPaletteMergeSpin->setToolTip(uiText(QStringLiteral("Two colors whose distance is under this value count as one color; smaller values keep small colored props.")));
    lineArtForm->addRow(uiText(QStringLiteral("Same color similarity")), m_lineArtPaletteMergeSpin);

    m_lineArtAdjacentMergeSpin = new QSpinBox(lineArtGroup);
    m_lineArtAdjacentMergeSpin->setObjectName(QStringLiteral("LineArtAdjacentMergeSpin"));
    m_lineArtAdjacentMergeSpin->setRange(0, 160);
    m_lineArtAdjacentMergeSpin->setValue(m_settings.lineArtAdjacentMerge);
    m_lineArtAdjacentMergeSpin->setFixedHeight(30);
    m_lineArtAdjacentMergeSpin->setToolTip(uiText(QStringLiteral("Adjacent blocks whose colors differ less than this are drawn as one block; 0 disables merging.")));
    lineArtForm->addRow(uiText(QStringLiteral("Adjacent color similarity")), m_lineArtAdjacentMergeSpin);

    m_lineArtMinAreaSpin = new QSpinBox(lineArtGroup);
    m_lineArtMinAreaSpin->setObjectName(QStringLiteral("LineArtMinAreaSpin"));
    m_lineArtMinAreaSpin->setRange(4, 400);
    m_lineArtMinAreaSpin->setSuffix(QStringLiteral(" px"));
    m_lineArtMinAreaSpin->setValue(m_settings.lineArtMinRegionArea);
    m_lineArtMinAreaSpin->setFixedHeight(30);
    m_lineArtMinAreaSpin->setToolTip(uiText(QStringLiteral("Blocks smaller than this area are skipped; lower it to trace tiny props.")));
    lineArtForm->addRow(uiText(QStringLiteral("Smallest block")), m_lineArtMinAreaSpin);

    // 色块过多时自适应降采样，默认勾选；取消勾选就按原始处理尺寸硬算。
    m_lineArtAdaptiveLimitSpin = new QSpinBox(lineArtGroup);
    m_lineArtAdaptiveLimitSpin->setObjectName(QStringLiteral("LineArtAdaptiveLimitSpin"));
    m_lineArtAdaptiveLimitSpin->setRange(100, 20000);
    m_lineArtAdaptiveLimitSpin->setSingleStep(100);
    m_lineArtAdaptiveLimitSpin->setSuffix(uiText(QStringLiteral(" blocks")));
    m_lineArtAdaptiveLimitSpin->setValue(m_settings.lineArtAdaptiveLimit);
    m_lineArtAdaptiveLimitSpin->setFixedHeight(30);
    m_lineArtAdaptiveLimitSpin->setToolTip(uiText(QStringLiteral(
        "Once the detected block count passes this value, the image is processed at a smaller size so the stroke count stays reasonable.")));
    lineArtForm->addRow(uiText(QStringLiteral("Block count limit")), m_lineArtAdaptiveLimitSpin);

    m_lineArtAdaptiveCheck = new QCheckBox(uiText(QStringLiteral("Auto downsample when exceeded")), lineArtGroup);
    m_lineArtAdaptiveCheck->setObjectName(QStringLiteral("LineArtAdaptiveCheck"));
    m_lineArtAdaptiveCheck->setChecked(m_settings.lineArtAdaptiveDownscale);
    m_lineArtAdaptiveCheck->setToolTip(uiText(QStringLiteral(
        "Checked by default: shrink the processing size instead of dropping blocks, so nothing disappears from the image. Uncheck to trace at full size.")));
    m_lineArtAdaptiveLimitSpin->setEnabled(m_lineArtAdaptiveCheck->isChecked());
    connect(m_lineArtAdaptiveCheck, &QCheckBox::toggled, m_lineArtAdaptiveLimitSpin, &QSpinBox::setEnabled);
    lineArtForm->addRow(QString(), m_lineArtAdaptiveCheck);

    // 档位 ↔ 具体数值：切档位就整组写入，手改任何一项就跳回“自定义”。
    auto matchesPreset = [this](const LineArtPreset& preset) {
        return m_lineArtLevelsSpin->value() == preset.levels
            && m_lineArtPaletteMergeSpin->value() == preset.palette
            && m_lineArtAdjacentMergeSpin->value() == preset.adjacent
            && m_lineArtAdjacentMergeSpin->value() == preset.adjacent
            && m_lineArtMinAreaSpin->value() == preset.minArea;
    };
    auto applyPreset = [this](int index) {
        const LineArtPreset preset = lineArtPreset(index);
        m_applyingPreset = true;
        m_lineArtLevelsSpin->setValue(preset.levels);
        m_lineArtPaletteMergeSpin->setValue(preset.palette);
        m_lineArtAdjacentMergeSpin->setValue(preset.adjacent);
        m_lineArtMinAreaSpin->setValue(preset.minArea);
        m_applyingPreset = false;
    };
    auto syncPresetCombo = [this, matchesPreset]() {
        int data = -1;
        for (int i = 0; i < lineArtPresetCount(); ++i) {
            if (matchesPreset(lineArtPreset(i))) {
                data = i;
                break;
            }
        }
        const int index = m_lineArtPresetCombo->findData(data);
        if (index < 0)
            return;
        QSignalBlocker blocker(m_lineArtPresetCombo);
        m_lineArtPresetCombo->setCurrentIndex(index);
    };

    connect(m_lineArtPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, applyPreset](int) {
                if (m_applyingPreset)
                    return;
                const int data = m_lineArtPresetCombo->currentData().toInt();
                if (data >= 0)
                    applyPreset(data);
            });
    for (QSpinBox* spin : { m_lineArtLevelsSpin, m_lineArtPaletteMergeSpin,
                            m_lineArtAdjacentMergeSpin, m_lineArtMinAreaSpin }) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, syncPresetCombo](int) {
                    if (m_applyingPreset)
                        return;
                    syncPresetCombo();
                });
    }
    syncPresetCombo();

    auto* lineArtHint = new QLabel(uiText(QStringLiteral(
        "Similarity values are RGB color distances (0-441): lower keeps more small props, higher merges more fragments. "
        "Adjacent merging only joins blocks that actually touch, so colour is the knob that changes the result.")),
        lineArtGroup);
    lineArtHint->setObjectName(QStringLiteral("DialogHint"));
    lineArtHint->setWordWrap(true);
    lineArtForm->addRow(QString(), lineArtHint);

    rootLayout->addWidget(lineArtGroup);

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
        m_settings.strokeSmoothing = m_strokeSmoothingSpin->value();
        m_settings.canvasSize = QSizeF(m_canvasWidthSpin->value(), m_canvasHeightSpin->value());
        m_settings.showGrid = m_gridCheck->isChecked();
        m_settings.lineArtColorLevels = m_lineArtLevelsSpin->value();
        m_settings.lineArtPaletteMerge = m_lineArtPaletteMergeSpin->value();
        m_settings.lineArtAdjacentMerge = m_lineArtAdjacentMergeSpin->value();
        m_settings.lineArtMinRegionArea = m_lineArtMinAreaSpin->value();
        m_settings.lineArtAdaptiveDownscale = m_lineArtAdaptiveCheck->isChecked();
        m_settings.lineArtAdaptiveLimit = m_lineArtAdaptiveLimitSpin->value();
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
    m_strokeSmoothingSpin->setValue(defaults.strokeSmoothing);
    m_canvasWidthSpin->setValue(static_cast<int>(defaults.canvasSize.width()));
    m_canvasHeightSpin->setValue(static_cast<int>(defaults.canvasSize.height()));
    m_gridCheck->setChecked(defaults.showGrid);
    m_lineArtLevelsSpin->setValue(defaults.lineArtColorLevels);
    m_lineArtPaletteMergeSpin->setValue(defaults.lineArtPaletteMerge);
    m_lineArtAdjacentMergeSpin->setValue(defaults.lineArtAdjacentMerge);
    m_lineArtMinAreaSpin->setValue(defaults.lineArtMinRegionArea);
    m_lineArtAdaptiveLimitSpin->setValue(defaults.lineArtAdaptiveLimit);
    m_lineArtAdaptiveCheck->setChecked(defaults.lineArtAdaptiveDownscale);
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
