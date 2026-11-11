#include "I18n.h"

#include "LineArtDialog.h"

#include "LineArtPresets.h"

#include <QComboBox>
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
}
