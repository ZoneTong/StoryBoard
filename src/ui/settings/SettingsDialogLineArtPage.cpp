#include "ui/settings/SettingsDialogLineArtPage.h"

#include "app/I18n.h"
#include "lineart/LineArtPresets.h"
#include "ui/settings/SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

SettingsDialogLineArtPage::SettingsDialogLineArtPage(const AppSettings& settings, QWidget* parent) : QWidget(parent)
{
    // 图片线稿化的参数：颜色怎么并、碎块怎么并，都要能自己调，否则遇到别的图只能重编译。
    // 上面再加一个档位，一键把下面几项换成一组实测过的组合。
    m_group = new QGroupBox(SettingsDialog::tr("Image outline tracing"), this);
    auto* lineArtLayout = new QVBoxLayout(m_group);
    lineArtLayout->setContentsMargins(12, 16, 12, 10);
    lineArtLayout->setSpacing(8);
    // 8 个数值项分左右两列排，行数减半；窄窗口下两列仍各占一半宽度，不会挤出横向滚动条。
    auto* fieldsRow = new QHBoxLayout();
    fieldsRow->setContentsMargins(0, 0, 0, 0);
    fieldsRow->setSpacing(18);
    auto* lineArtForm = new QFormLayout();
    lineArtForm->setContentsMargins(0, 0, 0, 0);
    lineArtForm->setSpacing(8);
    auto* lineArtFormRight = new QFormLayout();
    lineArtFormRight->setContentsMargins(0, 0, 0, 0);
    lineArtFormRight->setSpacing(8);
    fieldsRow->addLayout(lineArtForm, 1);
    fieldsRow->addLayout(lineArtFormRight, 1);
    lineArtLayout->addLayout(fieldsRow);

    m_presetCombo = new QComboBox(m_group);
    m_presetCombo->setObjectName(QStringLiteral("LineArtPresetCombo"));
    m_presetCombo->setFixedHeight(30);
    for (int i = 0; i < LineArtPresetCount(); ++i) {
        m_presetCombo->addItem(UiText(QString::fromLatin1(lineArtPreset(i).name)), i);
    }
    m_presetCombo->addItem(SettingsDialog::tr("Custom"), -1);
    m_presetCombo->setToolTip(SettingsDialog::tr(
        "Applies a tested parameter set to all items below; editing any value switches back to Custom."));
    lineArtForm->addRow(SettingsDialog::tr("Preset"), m_presetCombo);

    m_levelsSpin = new QSpinBox(m_group);
    m_levelsSpin->setObjectName(QStringLiteral("LineArtLevelsSpin"));
    m_levelsSpin->setRange(2, 12);
    m_levelsSpin->setValue(settings.lineArtColorLevels);
    m_levelsSpin->setFixedHeight(30);
    m_levelsSpin->setToolTip(SettingsDialog::tr(
        "How many steps each color channel is cut into; more steps keeps subtle shading but also more fragments."));
    lineArtForm->addRow(SettingsDialog::tr("Color levels"), m_levelsSpin);

    m_paletteMergeSpin = new QSpinBox(m_group);
    m_paletteMergeSpin->setObjectName(QStringLiteral("LineArtPaletteMergeSpin"));
    m_paletteMergeSpin->setRange(0, 160);
    m_paletteMergeSpin->setValue(settings.lineArtPaletteMerge);
    m_paletteMergeSpin->setFixedHeight(30);
    m_paletteMergeSpin->setToolTip(SettingsDialog::tr(
        "Two colors whose distance is under this value count as one color; smaller values keep small colored props."));
    lineArtForm->addRow(SettingsDialog::tr("Same color similarity"), m_paletteMergeSpin);

    m_adjacentMergeSpin = new QSpinBox(m_group);
    m_adjacentMergeSpin->setObjectName(QStringLiteral("LineArtAdjacentMergeSpin"));
    m_adjacentMergeSpin->setRange(0, 160);
    m_adjacentMergeSpin->setValue(settings.lineArtAdjacentMerge);
    m_adjacentMergeSpin->setFixedHeight(30);
    m_adjacentMergeSpin->setToolTip(SettingsDialog::tr(
        "Adjacent blocks whose colors differ less than this are drawn as one block; 0 disables merging."));
    lineArtForm->addRow(SettingsDialog::tr("Adjacent color similarity"), m_adjacentMergeSpin);

    m_minAreaSpin = new QSpinBox(m_group);
    m_minAreaSpin->setObjectName(QStringLiteral("LineArtMinAreaSpin"));
    m_minAreaSpin->setRange(4, 400);
    m_minAreaSpin->setSuffix(QStringLiteral(" px"));
    m_minAreaSpin->setValue(settings.lineArtMinRegionArea);
    m_minAreaSpin->setFixedHeight(30);
    m_minAreaSpin->setToolTip(
        SettingsDialog::tr("Blocks smaller than this area are skipped; lower it to trace tiny props."));
    lineArtFormRight->addRow(SettingsDialog::tr("Smallest block"), m_minAreaSpin);

    m_toleranceSpin = new QDoubleSpinBox(m_group);
    m_toleranceSpin->setObjectName(QStringLiteral("LineArtToleranceSpin"));
    m_toleranceSpin->setRange(0.0, 4.0);
    m_toleranceSpin->setSingleStep(0.1);
    m_toleranceSpin->setDecimals(2);
    m_toleranceSpin->setSuffix(QStringLiteral(" px"));
    m_toleranceSpin->setValue(settings.lineArtSimplifyTolerance);
    m_toleranceSpin->setFixedHeight(30);
    m_toleranceSpin->setToolTip(
        SettingsDialog::tr("How far the traced outline may deviate from its points; smaller follows "
                           "pixel steps, larger turns arcs into fewer, straighter segments."));
    lineArtFormRight->addRow(SettingsDialog::tr("Simplify tolerance"), m_toleranceSpin);

    m_thicknessSpin = new QDoubleSpinBox(m_group);
    m_thicknessSpin->setObjectName(QStringLiteral("LineArtThicknessSpin"));
    m_thicknessSpin->setRange(0.0, 8.0);
    m_thicknessSpin->setSingleStep(0.1);
    m_thicknessSpin->setDecimals(2);
    m_thicknessSpin->setSuffix(QStringLiteral(" px"));
    m_thicknessSpin->setValue(settings.lineArtMinThickness);
    m_thicknessSpin->setFixedHeight(30);
    m_thicknessSpin->setToolTip(
        SettingsDialog::tr("Blocks whose average width is under this value are not traced; 0 keeps "
                           "every thin band, higher drops antialiasing halos."));
    lineArtFormRight->addRow(SettingsDialog::tr("Thinnest band"), m_thicknessSpin);

    // 色块过多时自适应降采样，默认勾选；取消勾选就按原始处理尺寸硬算。
    m_adaptiveLimitSpin = new QSpinBox(m_group);
    m_adaptiveLimitSpin->setObjectName(QStringLiteral("LineArtAdaptiveLimitSpin"));
    m_adaptiveLimitSpin->setRange(100, 20000);
    m_adaptiveLimitSpin->setSingleStep(100);
    m_adaptiveLimitSpin->setSuffix(SettingsDialog::tr(" blocks"));
    m_adaptiveLimitSpin->setValue(settings.lineArtAdaptiveLimit);
    m_adaptiveLimitSpin->setFixedHeight(30);
    m_adaptiveLimitSpin->setToolTip(
        SettingsDialog::tr("Once the detected block count passes this value, the image is processed "
                           "at a smaller size so the stroke count stays reasonable."));
    lineArtFormRight->addRow(SettingsDialog::tr("Block count limit"), m_adaptiveLimitSpin);

    m_adaptiveCheck = new QCheckBox(SettingsDialog::tr("Auto downsample when exceeded"), m_group);
    m_adaptiveCheck->setObjectName(QStringLiteral("LineArtAdaptiveCheck"));
    m_adaptiveCheck->setChecked(settings.lineArtAdaptiveDownscale);
    m_adaptiveCheck->setToolTip(
        SettingsDialog::tr("Checked by default: shrink the processing size instead of dropping blocks, "
                           "so nothing disappears from the image. Uncheck to trace at full size."));
    m_adaptiveLimitSpin->setEnabled(m_adaptiveCheck->isChecked());
    connect(m_adaptiveCheck, &QCheckBox::toggled, m_adaptiveLimitSpin, &QSpinBox::setEnabled);

    // 档位 ↔ 具体数值：切档位就整组写入，手改任何一项就跳回“自定义”。
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_applyingPreset) {
            return;
        }
        const int data = m_presetCombo->currentData().toInt();
        if (data >= 0) {
            ApplyPreset(data);
        }
    });
    for (QSpinBox* spin : {m_levelsSpin, m_paletteMergeSpin, m_adjacentMergeSpin, m_minAreaSpin}) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            if (m_applyingPreset) {
                return;
            }
            SyncPresetCombo();
        });
    }
    // 手改简化容差同样要跳回“自定义”。
    connect(m_toleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (m_applyingPreset) {
            return;
        }
        SyncPresetCombo();
    });
    SyncPresetCombo();

    auto* lineArtHint =
        new QLabel(SettingsDialog::tr(
                       "<ul>"
                       "<li>Color similarity: RGB color distance (0-441). Lower keeps more small props, higher merges "
                       "more fragments; adjacent merging only joins blocks that actually touch.</li>"
                       "<li>Simplify tolerance: how far a traced point may move. Smaller follows pixel steps, larger "
                       "flattens arcs into straight segments.</li>"
                       "<li>Thinnest band: drops antialiasing halos; 0 keeps every thin band, higher also drops "
                       "genuinely thin lines.</li>"
                       "<li>Block count limit: over the limit the image is traced at a smaller size instead of losing "
                       "blocks. Uncheck to trace at full size (slower on busy images).</li>"
                       "<li>Saved with this dialog: these values become the defaults for the import dialog, which can "
                       "overwrite them with its own Save to configuration file button.</li>"
                       "</ul>"),
                   m_group);
    lineArtHint->setTextFormat(Qt::RichText);
    lineArtHint->setObjectName(QStringLiteral("DialogHint"));
    lineArtHint->setWordWrap(true);

    // 复选框和「怎么调」放同一行，省一行高度；说明默认展开，点一下即可折叠。
    m_hintToggle = new QToolButton(m_group);
    m_hintToggle->setObjectName(QStringLiteral("LineArtHintToggle"));
    m_hintToggle->setCheckable(true);
    m_hintToggle->setChecked(true);
    m_hintToggle->setCursor(Qt::PointingHandCursor);
    m_hintToggle->setText(QStringLiteral("▾ ") + SettingsDialog::tr("How to tune"));
    auto* lineArtFooter = new QHBoxLayout();
    lineArtFooter->setContentsMargins(0, 0, 0, 0);
    lineArtFooter->setSpacing(12);
    lineArtFooter->addWidget(m_adaptiveCheck);
    lineArtFooter->addStretch(1);
    lineArtFooter->addWidget(m_hintToggle);
    lineArtLayout->addLayout(lineArtFooter);
    lineArtLayout->addWidget(lineArtHint);
    connect(m_hintToggle, &QToolButton::toggled, this, [this, lineArtHint](bool expanded) {
        lineArtHint->setVisible(expanded);
        m_hintToggle->setText((expanded ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) +
                              SettingsDialog::tr("How to tune"));
    });
}

bool SettingsDialogLineArtPage::MatchesPreset(const LineArtPreset& preset) const
{
    return m_levelsSpin->value() == preset.levels && m_paletteMergeSpin->value() == preset.palette &&
           m_adjacentMergeSpin->value() == preset.adjacent &&
           m_minAreaSpin->value() == preset.minArea
           // 容差是浮点，比较留 1e-3 余量，免得 0.6000000001 被判成“自定义”。
           && qAbs(m_toleranceSpin->value() - preset.tolerance) < 1e-3;
}

void SettingsDialogLineArtPage::ApplyPreset(int index)
{
    const LineArtPreset preset = lineArtPreset(index);
    m_applyingPreset = true;
    m_levelsSpin->setValue(preset.levels);
    m_paletteMergeSpin->setValue(preset.palette);
    m_adjacentMergeSpin->setValue(preset.adjacent);
    m_minAreaSpin->setValue(preset.minArea);
    m_toleranceSpin->setValue(preset.tolerance);
    m_applyingPreset = false;
}

void SettingsDialogLineArtPage::SyncPresetCombo()
{
    int data = -1;
    for (int i = 0; i < LineArtPresetCount(); ++i) {
        if (MatchesPreset(lineArtPreset(i))) {
            data = i;
            break;
        }
    }
    const int index = m_presetCombo->findData(data);
    if (index < 0) {
        return;
    }
    QSignalBlocker blocker(m_presetCombo);
    m_presetCombo->setCurrentIndex(index);
}

void SettingsDialogLineArtPage::WriteTo(AppSettings& settings) const
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

void SettingsDialogLineArtPage::Reset(const AppSettings& defaults)
{
    m_levelsSpin->setValue(defaults.lineArtColorLevels);
    m_paletteMergeSpin->setValue(defaults.lineArtPaletteMerge);
    m_adjacentMergeSpin->setValue(defaults.lineArtAdjacentMerge);
    m_minAreaSpin->setValue(defaults.lineArtMinRegionArea);
    m_adaptiveLimitSpin->setValue(defaults.lineArtAdaptiveLimit);
    m_adaptiveCheck->setChecked(defaults.lineArtAdaptiveDownscale);
    m_toleranceSpin->setValue(defaults.lineArtSimplifyTolerance);
    m_thicknessSpin->setValue(defaults.lineArtMinThickness);
    SyncPresetCombo();
}
