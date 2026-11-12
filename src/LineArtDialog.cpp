#include "I18n.h"

#include "LineArtDialog.h"

#include "LineArtPresets.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

LineArtDialog::LineArtDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(uiText(QStringLiteral("Image outline tracing")));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setModal(true);
    setMinimumWidth(460);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto* title = new QLabel(uiText(QStringLiteral("Image outline tracing")), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    rootLayout->addWidget(title);

    auto* lineArtGroup = new QGroupBox(uiText(QStringLiteral("Image outline tracing")), this);
    auto* lineArtForm = new QFormLayout(lineArtGroup);
    lineArtForm->setContentsMargins(16, 18, 16, 16);
    lineArtForm->setSpacing(10);

    m_presetCombo = new QComboBox(lineArtGroup);
    m_presetCombo->setObjectName(QStringLiteral("LineArtPresetCombo"));
    for (int i = 0; i < lineArtPresetCount(); ++i)
        m_presetCombo->addItem(uiText(QString::fromLatin1(lineArtPreset(i).name)), i);
    m_presetCombo->addItem(uiText(QStringLiteral("Custom")), -1);
    m_presetCombo->setToolTip(uiText(QStringLiteral("Applies a tested parameter set to all items below; editing any value switches back to Custom.")));
    lineArtForm->addRow(uiText(QStringLiteral("Preset")), m_presetCombo);

    m_levelsSpin = new QSpinBox(lineArtGroup);
    m_levelsSpin->setObjectName(QStringLiteral("LineArtLevelsSpin"));
    m_levelsSpin->setRange(2, 12);
    m_levelsSpin->setValue(settings.lineArtColorLevels);
    m_levelsSpin->setFixedHeight(30);
    m_levelsSpin->setToolTip(uiText(QStringLiteral("How many steps each color channel is cut into; more steps keeps subtle shading but also more fragments.")));
    lineArtForm->addRow(uiText(QStringLiteral("Color levels")), m_levelsSpin);

    m_paletteMergeSpin = new QSpinBox(lineArtGroup);
    m_paletteMergeSpin->setObjectName(QStringLiteral("LineArtPaletteMergeSpin"));
    m_paletteMergeSpin->setRange(0, 160);
    m_paletteMergeSpin->setValue(settings.lineArtPaletteMerge);
    m_paletteMergeSpin->setFixedHeight(30);
    m_paletteMergeSpin->setToolTip(uiText(QStringLiteral("Two colors whose distance is under this value count as one color; smaller values keep small colored props.")));
    lineArtForm->addRow(uiText(QStringLiteral("Same color similarity")), m_paletteMergeSpin);

    m_adjacentMergeSpin = new QSpinBox(lineArtGroup);
    m_adjacentMergeSpin->setObjectName(QStringLiteral("LineArtAdjacentMergeSpin"));
    m_adjacentMergeSpin->setRange(0, 160);
    m_adjacentMergeSpin->setValue(settings.lineArtAdjacentMerge);
    m_adjacentMergeSpin->setFixedHeight(30);
    m_adjacentMergeSpin->setToolTip(uiText(QStringLiteral("Adjacent blocks whose colors differ less than this are drawn as one block; 0 disables merging.")));
    lineArtForm->addRow(uiText(QStringLiteral("Adjacent color similarity")), m_adjacentMergeSpin);

    m_minAreaSpin = new QSpinBox(lineArtGroup);
    m_minAreaSpin->setObjectName(QStringLiteral("LineArtMinAreaSpin"));
    m_minAreaSpin->setRange(4, 400);
    m_minAreaSpin->setSuffix(QStringLiteral(" px"));
    m_minAreaSpin->setValue(settings.lineArtMinRegionArea);
    m_minAreaSpin->setFixedHeight(30);
    m_minAreaSpin->setToolTip(uiText(QStringLiteral("Blocks smaller than this area are skipped; lower it to trace tiny props.")));
    lineArtForm->addRow(uiText(QStringLiteral("Smallest block")), m_minAreaSpin);

    m_toleranceSpin = new QDoubleSpinBox(lineArtGroup);
    m_toleranceSpin->setObjectName(QStringLiteral("LineArtToleranceSpin"));
    m_toleranceSpin->setRange(0.0, 4.0);
    m_toleranceSpin->setSingleStep(0.1);
    m_toleranceSpin->setDecimals(2);
    m_toleranceSpin->setSuffix(QStringLiteral(" px"));
    m_toleranceSpin->setValue(settings.lineArtSimplifyTolerance);
    m_toleranceSpin->setFixedHeight(30);
    m_toleranceSpin->setToolTip(uiText(QStringLiteral(
        "How far the traced outline may deviate from its points; smaller follows pixel steps, larger turns arcs into fewer, straighter segments.")));
    lineArtForm->addRow(uiText(QStringLiteral("Simplify tolerance")), m_toleranceSpin);

    m_thicknessSpin = new QDoubleSpinBox(lineArtGroup);
    m_thicknessSpin->setObjectName(QStringLiteral("LineArtThicknessSpin"));
    m_thicknessSpin->setRange(0.0, 8.0);
    m_thicknessSpin->setSingleStep(0.1);
    m_thicknessSpin->setDecimals(2);
    m_thicknessSpin->setSuffix(QStringLiteral(" px"));
    m_thicknessSpin->setValue(settings.lineArtMinThickness);
    m_thicknessSpin->setFixedHeight(30);
    m_thicknessSpin->setToolTip(uiText(QStringLiteral(
        "Blocks whose average width is under this value are not traced; 0 keeps every thin band, higher drops antialiasing halos.")));
    lineArtForm->addRow(uiText(QStringLiteral("Thinnest band")), m_thicknessSpin);

    // 色块过多时自适应降采样：默认勾选，取消勾选就按原始处理尺寸硬算（色块多时会变慢）。
    m_adaptiveLimitSpin = new QSpinBox(lineArtGroup);
    m_adaptiveLimitSpin->setObjectName(QStringLiteral("LineArtAdaptiveLimitSpin"));
    m_adaptiveLimitSpin->setRange(100, 20000);
    m_adaptiveLimitSpin->setSingleStep(100);
    m_adaptiveLimitSpin->setSuffix(uiText(QStringLiteral(" blocks")));
    m_adaptiveLimitSpin->setValue(settings.lineArtAdaptiveLimit);
    m_adaptiveLimitSpin->setFixedHeight(30);
    m_adaptiveLimitSpin->setToolTip(uiText(QStringLiteral(
        "Once the detected block count passes this value, the image is processed at a smaller size so the stroke count stays reasonable.")));
    lineArtForm->addRow(uiText(QStringLiteral("Block count limit")), m_adaptiveLimitSpin);

    m_adaptiveCheck = new QCheckBox(uiText(QStringLiteral("Auto downsample when exceeded")), lineArtGroup);
    m_adaptiveCheck->setObjectName(QStringLiteral("LineArtAdaptiveCheck"));
    m_adaptiveCheck->setChecked(settings.lineArtAdaptiveDownscale);
    m_adaptiveCheck->setToolTip(uiText(QStringLiteral(
        "Checked by default: shrink the processing size instead of dropping blocks, so nothing disappears from the image. Uncheck to trace at full size.")));
    m_adaptiveLimitSpin->setEnabled(m_adaptiveCheck->isChecked());
    connect(m_adaptiveCheck, &QCheckBox::toggled, m_adaptiveLimitSpin, &QSpinBox::setEnabled);
    lineArtForm->addRow(QString(), m_adaptiveCheck);

    rootLayout->addWidget(lineArtGroup);

    auto* hint = new QLabel(uiText(QStringLiteral(
        "Similarity values are RGB color distances (0-441): lower keeps more small props, higher merges more fragments. "
        "Adjacent merging only joins blocks that actually touch, so colour is the knob that changes the result. "
        "Stroke width is fixed at 1.")),
        this);
    hint->setObjectName(QStringLiteral("DialogHint"));
    hint->setWordWrap(true);
    rootLayout->addWidget(hint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(QStringLiteral("OK")));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(QStringLiteral("Cancel")));

    // 需求：参数默认只改内存，本次进程内生效；只有点这个按钮才把当前参数覆盖进配置文件。
    m_saveToConfigButton = new QPushButton(uiText(QStringLiteral("Save to configuration file")), this);
    m_saveToConfigButton->setObjectName(QStringLiteral("LineArtSaveSettingsButton"));
    m_saveToConfigButton->setToolTip(uiText(QStringLiteral(
        "Write the parameters above into StoryBoard.ini now; otherwise they only apply to this session.")));
    buttons->addButton(m_saveToConfigButton, QDialogButtonBox::ActionRole);
    connect(m_saveToConfigButton, &QPushButton::clicked, this, [this]() {
        m_saveToConfig = true;
        accept();
    });

    rootLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 档位 ↔ 具体数值：切档位就整组写入，手改任何一项就跳回“自定义”。
    auto matchesPreset = [this](const LineArtPreset& preset) {
        return m_levelsSpin->value() == preset.levels
            && m_paletteMergeSpin->value() == preset.palette
            && m_adjacentMergeSpin->value() == preset.adjacent
            && m_minAreaSpin->value() == preset.minArea;
    };
    auto applyPreset = [this](int index) {
        const LineArtPreset preset = lineArtPreset(index);
        m_applyingPreset = true;
        m_levelsSpin->setValue(preset.levels);
        m_paletteMergeSpin->setValue(preset.palette);
        m_adjacentMergeSpin->setValue(preset.adjacent);
        m_minAreaSpin->setValue(preset.minArea);
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
        const int index = m_presetCombo->findData(data);
        if (index < 0)
            return;
        QSignalBlocker blocker(m_presetCombo);
        m_presetCombo->setCurrentIndex(index);
    };

    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, applyPreset](int) {
                if (m_applyingPreset)
                    return;
                const int data = m_presetCombo->currentData().toInt();
                if (data >= 0)
                    applyPreset(data);
            });
    for (QSpinBox* spin : { m_levelsSpin, m_paletteMergeSpin, m_adjacentMergeSpin, m_minAreaSpin }) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, syncPresetCombo](int) {
                    if (m_applyingPreset)
                        return;
                    syncPresetCombo();
                });
    }
    syncPresetCombo();
}

LineArtConverter::Options LineArtDialog::options() const
{
    LineArtConverter::Options options;
    options.colorLevels = m_levelsSpin->value();
    options.paletteMergeDistance = m_paletteMergeSpin->value();
    options.adjacentMergeDistance = m_adjacentMergeSpin->value();
    options.minRegionArea = m_minAreaSpin->value();
    options.adaptiveDownscale = m_adaptiveCheck->isChecked();
    options.adaptiveRegionLimit = m_adaptiveLimitSpin->value();
    options.simplifyTolerance = m_toleranceSpin->value();
    options.minRegionThickness = m_thicknessSpin->value();
    // 需求：线稿线宽固定为 1，不随任何设置变化。
    options.strokeWidth = 1.0;
    return options;
}

void LineArtDialog::applyToSettings(AppSettings& settings) const
{
    settings.lineArtColorLevels = m_levelsSpin->value();
    settings.lineArtPaletteMerge = m_paletteMergeSpin->value();
    settings.lineArtAdjacentMerge = m_adjacentMergeSpin->value();
    settings.lineArtMinRegionArea = m_minAreaSpin->value();
    settings.lineArtAdaptiveDownscale = m_adaptiveCheck->isChecked();
    settings.lineArtAdaptiveLimit = m_adaptiveLimitSpin->value();
    settings.lineArtSimplifyTolerance = m_toleranceSpin->value();
    settings.lineArtMinThickness = m_thicknessSpin->value();
}
