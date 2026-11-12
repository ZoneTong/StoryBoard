#include "I18n.h"

#include "SettingsDialog.h"

#include "IconFactory.h"
#include "LineArtPresets.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
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
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(600);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto* title = new QLabel(tr("Board settings"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(title);

    auto* hint = new QLabel(tr("These settings become the defaults for new brushes and canvases and are saved automatically."), this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    rootLayout->addWidget(hint);

    auto* languageGroup = new QGroupBox(tr("Interface language"), this);
    auto* languageForm = new QFormLayout(languageGroup);
    languageForm->setContentsMargins(16, 18, 16, 16);
    languageForm->setSpacing(10);
    m_languageCombo = new QComboBox(languageGroup);
    m_languageCombo->addItem(tr("Follow system"), static_cast<int>(UiLanguage::System));
    m_languageCombo->addItem(tr("Simplified Chinese"), static_cast<int>(UiLanguage::Chinese));
    m_languageCombo->addItem(QStringLiteral("English"), static_cast<int>(UiLanguage::English));
    m_languageCombo->setCurrentIndex(m_languageCombo->findData(static_cast<int>(uiLanguageSetting())));
    languageForm->addRow(tr("Language"), m_languageCombo);
    languageForm->addRow(QString(), new QLabel(tr("The interface updates immediately and this choice is remembered."), languageGroup));
    rootLayout->addWidget(languageGroup);

    auto* brushGroup = new QGroupBox(tr("Brush defaults"), this);
    auto* brushForm = new QFormLayout(brushGroup);
    brushForm->setContentsMargins(16, 18, 16, 16);
    brushForm->setSpacing(10);

    m_brushColorButton = new QPushButton(brushGroup);
    m_brushColorButton->setIconSize(QSize(18, 18));
    brushForm->addRow(tr("Default color"), m_brushColorButton);

    m_brushWidthSpin = new QSpinBox(brushGroup);
    m_brushWidthSpin->setRange(1, 64);
    m_brushWidthSpin->setSuffix(QStringLiteral(" px"));
    m_brushWidthSpin->setValue(m_settings.brushWidth);
    m_brushWidthSpin->setFixedHeight(30);
    brushForm->addRow(tr("Default width"), m_brushWidthSpin);

    m_strokeSmoothingSpin = new QSpinBox(brushGroup);
    m_strokeSmoothingSpin->setObjectName(QStringLiteral("StrokeSmoothingSpin"));
    m_strokeSmoothingSpin->setRange(0, 30);
    m_strokeSmoothingSpin->setSuffix(QStringLiteral(" px"));
    m_strokeSmoothingSpin->setValue(m_settings.strokeSmoothing);
    m_strokeSmoothingSpin->setFixedHeight(30);
    m_strokeSmoothingSpin->setToolTip(tr("0 disables automatic smoothing; larger values tolerate more hand jitter and deviation."));
    brushForm->addRow(tr("Auto-smooth tolerance"), m_strokeSmoothingSpin);

    auto* smoothingHint = new QLabel(tr("0 disables automatic smoothing; larger values tolerate more hand jitter and deviation."), brushGroup);
    smoothingHint->setObjectName(QStringLiteral("DialogHint"));
    smoothingHint->setWordWrap(true);
    brushForm->addRow(QString(), smoothingHint);

    rootLayout->addWidget(brushGroup);

    auto* canvasGroup = new QGroupBox(tr("Canvas"), this);
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
    canvasForm->addRow(tr("Canvas size"), sizeRow);

    m_gridCheck = new QCheckBox(tr("Show helper grid"), canvasGroup);
    m_gridCheck->setChecked(m_settings.showGrid);
    canvasForm->addRow(QString(), m_gridCheck);

    m_canvasColorButton = new QPushButton(canvasGroup);
    m_canvasColorButton->setIconSize(QSize(18, 18));
    canvasForm->addRow(tr("Canvas surround color"), m_canvasColorButton);

    auto* canvasHint = new QLabel(tr("Changing the canvas size does not move existing items; undo restores it."), canvasGroup);
    canvasHint->setObjectName(QStringLiteral("DialogHint"));
    canvasHint->setWordWrap(true);
    canvasForm->addRow(QString(), canvasHint);

    rootLayout->addWidget(canvasGroup);

    // 图片线稿化的参数：颜色怎么并、碎块怎么并，都要能自己调，否则遇到别的图只能重编译。
    // 上面再加一个档位，一键把下面几项换成一组实测过的组合。
    auto* lineArtGroup = new QGroupBox(tr("Image outline tracing"), this);
    auto* lineArtForm = new QFormLayout(lineArtGroup);
    lineArtForm->setContentsMargins(16, 18, 16, 16);
    lineArtForm->setSpacing(10);

    m_lineArtPresetCombo = new QComboBox(lineArtGroup);
    m_lineArtPresetCombo->setObjectName(QStringLiteral("LineArtPresetCombo"));
    m_lineArtPresetCombo->setFixedHeight(30);
    for (int i = 0; i < lineArtPresetCount(); ++i)
        m_lineArtPresetCombo->addItem(uiText(QString::fromLatin1(lineArtPreset(i).name)), i);
    m_lineArtPresetCombo->addItem(tr("Custom"), -1);
    m_lineArtPresetCombo->setToolTip(tr("Applies a tested parameter set to all items below; editing any value switches back to Custom."));
    lineArtForm->addRow(tr("Preset"), m_lineArtPresetCombo);

    m_lineArtLevelsSpin = new QSpinBox(lineArtGroup);
    m_lineArtLevelsSpin->setObjectName(QStringLiteral("LineArtLevelsSpin"));
    m_lineArtLevelsSpin->setRange(2, 12);
    m_lineArtLevelsSpin->setValue(m_settings.lineArtColorLevels);
    m_lineArtLevelsSpin->setFixedHeight(30);
    m_lineArtLevelsSpin->setToolTip(tr("How many steps each color channel is cut into; more steps keeps subtle shading but also more fragments."));
    lineArtForm->addRow(tr("Color levels"), m_lineArtLevelsSpin);

    m_lineArtPaletteMergeSpin = new QSpinBox(lineArtGroup);
    m_lineArtPaletteMergeSpin->setObjectName(QStringLiteral("LineArtPaletteMergeSpin"));
    m_lineArtPaletteMergeSpin->setRange(0, 160);
    m_lineArtPaletteMergeSpin->setValue(m_settings.lineArtPaletteMerge);
    m_lineArtPaletteMergeSpin->setFixedHeight(30);
    m_lineArtPaletteMergeSpin->setToolTip(tr("Two colors whose distance is under this value count as one color; smaller values keep small colored props."));
    lineArtForm->addRow(tr("Same color similarity"), m_lineArtPaletteMergeSpin);

    m_lineArtAdjacentMergeSpin = new QSpinBox(lineArtGroup);
    m_lineArtAdjacentMergeSpin->setObjectName(QStringLiteral("LineArtAdjacentMergeSpin"));
    m_lineArtAdjacentMergeSpin->setRange(0, 160);
    m_lineArtAdjacentMergeSpin->setValue(m_settings.lineArtAdjacentMerge);
    m_lineArtAdjacentMergeSpin->setFixedHeight(30);
    m_lineArtAdjacentMergeSpin->setToolTip(tr("Adjacent blocks whose colors differ less than this are drawn as one block; 0 disables merging."));
    lineArtForm->addRow(tr("Adjacent color similarity"), m_lineArtAdjacentMergeSpin);

    m_lineArtMinAreaSpin = new QSpinBox(lineArtGroup);
    m_lineArtMinAreaSpin->setObjectName(QStringLiteral("LineArtMinAreaSpin"));
    m_lineArtMinAreaSpin->setRange(4, 400);
    m_lineArtMinAreaSpin->setSuffix(QStringLiteral(" px"));
    m_lineArtMinAreaSpin->setValue(m_settings.lineArtMinRegionArea);
    m_lineArtMinAreaSpin->setFixedHeight(30);
    m_lineArtMinAreaSpin->setToolTip(tr("Blocks smaller than this area are skipped; lower it to trace tiny props."));
    lineArtForm->addRow(tr("Smallest block"), m_lineArtMinAreaSpin);

    m_lineArtToleranceSpin = new QDoubleSpinBox(lineArtGroup);
    m_lineArtToleranceSpin->setObjectName(QStringLiteral("LineArtToleranceSpin"));
    m_lineArtToleranceSpin->setRange(0.0, 4.0);
    m_lineArtToleranceSpin->setSingleStep(0.1);
    m_lineArtToleranceSpin->setDecimals(2);
    m_lineArtToleranceSpin->setSuffix(QStringLiteral(" px"));
    m_lineArtToleranceSpin->setValue(m_settings.lineArtSimplifyTolerance);
    m_lineArtToleranceSpin->setFixedHeight(30);
    m_lineArtToleranceSpin->setToolTip(tr("How far the traced outline may deviate from its points; smaller follows pixel steps, larger turns arcs into fewer, straighter segments."));
    lineArtForm->addRow(tr("Simplify tolerance"), m_lineArtToleranceSpin);

    m_lineArtThicknessSpin = new QDoubleSpinBox(lineArtGroup);
    m_lineArtThicknessSpin->setObjectName(QStringLiteral("LineArtThicknessSpin"));
    m_lineArtThicknessSpin->setRange(0.0, 8.0);
    m_lineArtThicknessSpin->setSingleStep(0.1);
    m_lineArtThicknessSpin->setDecimals(2);
    m_lineArtThicknessSpin->setSuffix(QStringLiteral(" px"));
    m_lineArtThicknessSpin->setValue(m_settings.lineArtMinThickness);
    m_lineArtThicknessSpin->setFixedHeight(30);
    m_lineArtThicknessSpin->setToolTip(tr("Blocks whose average width is under this value are not traced; 0 keeps every thin band, higher drops antialiasing halos."));
    lineArtForm->addRow(tr("Thinnest band"), m_lineArtThicknessSpin);

    // 色块过多时自适应降采样，默认勾选；取消勾选就按原始处理尺寸硬算。
    m_lineArtAdaptiveLimitSpin = new QSpinBox(lineArtGroup);
    m_lineArtAdaptiveLimitSpin->setObjectName(QStringLiteral("LineArtAdaptiveLimitSpin"));
    m_lineArtAdaptiveLimitSpin->setRange(100, 20000);
    m_lineArtAdaptiveLimitSpin->setSingleStep(100);
    m_lineArtAdaptiveLimitSpin->setSuffix(tr(" blocks"));
    m_lineArtAdaptiveLimitSpin->setValue(m_settings.lineArtAdaptiveLimit);
    m_lineArtAdaptiveLimitSpin->setFixedHeight(30);
    m_lineArtAdaptiveLimitSpin->setToolTip(tr("Once the detected block count passes this value, the image is processed at a smaller size so the stroke count stays reasonable."));
    lineArtForm->addRow(tr("Block count limit"), m_lineArtAdaptiveLimitSpin);

    m_lineArtAdaptiveCheck = new QCheckBox(tr("Auto downsample when exceeded"), lineArtGroup);
    m_lineArtAdaptiveCheck->setObjectName(QStringLiteral("LineArtAdaptiveCheck"));
    m_lineArtAdaptiveCheck->setChecked(m_settings.lineArtAdaptiveDownscale);
    m_lineArtAdaptiveCheck->setToolTip(tr("Checked by default: shrink the processing size instead of dropping blocks, so nothing disappears from the image. Uncheck to trace at full size."));
    m_lineArtAdaptiveLimitSpin->setEnabled(m_lineArtAdaptiveCheck->isChecked());
    connect(m_lineArtAdaptiveCheck, &QCheckBox::toggled, m_lineArtAdaptiveLimitSpin, &QSpinBox::setEnabled);
    lineArtForm->addRow(QString(), m_lineArtAdaptiveCheck);

    // 档位 ↔ 具体数值：切档位就整组写入，手改任何一项就跳回“自定义”。
    auto matchesPreset = [this](const LineArtPreset& preset) {
        return m_lineArtLevelsSpin->value() == preset.levels
            && m_lineArtPaletteMergeSpin->value() == preset.palette
            && m_lineArtAdjacentMergeSpin->value() == preset.adjacent
            && m_lineArtMinAreaSpin->value() == preset.minArea
            // 容差是浮点，比较留 1e-3 余量，免得 0.6000000001 被判成“自定义”。
            && qAbs(m_lineArtToleranceSpin->value() - preset.tolerance) < 1e-3;
    };
    auto applyPreset = [this](int index) {
        const LineArtPreset preset = lineArtPreset(index);
        m_applyingPreset = true;
        m_lineArtLevelsSpin->setValue(preset.levels);
        m_lineArtPaletteMergeSpin->setValue(preset.palette);
        m_lineArtAdjacentMergeSpin->setValue(preset.adjacent);
        m_lineArtMinAreaSpin->setValue(preset.minArea);
        m_lineArtToleranceSpin->setValue(preset.tolerance);
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
    // 手改简化容差同样要跳回“自定义”。
    connect(m_lineArtToleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, syncPresetCombo](double) {
                if (m_applyingPreset)
                    return;
                syncPresetCombo();
            });
    syncPresetCombo();

    auto* lineArtHint = new QLabel(tr("<b>How to tune</b><ul>"
        "<li>Color similarity: RGB color distance (0-441). Lower keeps more small props, higher merges more fragments; adjacent merging only joins blocks that actually touch.</li>"
        "<li>Simplify tolerance: how far a traced point may move. Smaller follows pixel steps, larger flattens arcs into straight segments.</li>"
        "<li>Thinnest band: drops antialiasing halos; 0 keeps every thin band, higher also drops genuinely thin lines.</li>"
        "<li>Block count limit: over the limit the image is traced at a smaller size instead of losing blocks. Uncheck to trace at full size (slower on busy images).</li>"
        "<li>Saved with this dialog: these values become the defaults for the import dialog, which can overwrite them with its own Save to configuration file button.</li>"
        "</ul>"),
        lineArtGroup);
    lineArtHint->setTextFormat(Qt::RichText);
    lineArtHint->setObjectName(QStringLiteral("DialogHint"));
    lineArtHint->setWordWrap(true);
    lineArtForm->addRow(QString(), lineArtHint);

    rootLayout->addWidget(lineArtGroup);

    auto* storageGroup = new QGroupBox(tr("Canvas storage"), this);
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
    auto* openButton = new QPushButton(tr("Open folder"), storageGroup);
    storageLayout->addWidget(pathEdit, 1);
    storageLayout->addWidget(openButton);

    rootLayout->addWidget(storageGroup);
    rootLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::Reset,
                                         this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::Reset)->setText(tr("Restore defaults"));
    rootLayout->addWidget(buttons);

    refreshColorButton(m_brushColorButton, m_settings.brushColor);
    refreshColorButton(m_canvasColorButton, m_settings.canvasColor);

    connect(m_brushColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = m_settings.brushColor;
        if (pickColor(color, tr("Choose the default brush color"), true))
            setBrushColor(color);
    });
    connect(m_canvasColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = m_settings.canvasColor;
        if (pickColor(color, tr("Choose canvas surround color"), false))
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
        m_settings.lineArtSimplifyTolerance = m_lineArtToleranceSpin->value();
        m_settings.lineArtMinThickness = m_lineArtThicknessSpin->value();
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
    m_lineArtToleranceSpin->setValue(defaults.lineArtSimplifyTolerance);
    m_lineArtThicknessSpin->setValue(defaults.lineArtMinThickness);
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
