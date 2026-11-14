#pragma once

#include "LineArtPresets.h"
#include "Settings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QSpinBox;
class QToolButton;

// 设置窗的图片线稿化参数页：档位预设与颜色合并、碎块面积、简化容差、
// 最细条带、自适应降采样等数值项；手改任意一项自动回到“自定义”档位。
class SettingsDialogLineArtPage : public QWidget {
    Q_OBJECT

  public:
    explicit SettingsDialogLineArtPage(const AppSettings& settings, QWidget* parent = nullptr);

    QGroupBox* group() const { return m_group; }

    void writeTo(AppSettings& settings) const;
    void reset(const AppSettings& defaults);

  private:
    bool matchesPreset(const LineArtPreset& preset) const;
    void applyPreset(int index);
    void syncPresetCombo();

    bool m_applyingPreset = false;
    QGroupBox* m_group = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_levelsSpin = nullptr;
    QSpinBox* m_paletteMergeSpin = nullptr;
    QSpinBox* m_adjacentMergeSpin = nullptr;
    QSpinBox* m_minAreaSpin = nullptr;
    QDoubleSpinBox* m_toleranceSpin = nullptr;
    QDoubleSpinBox* m_thicknessSpin = nullptr;
    QSpinBox* m_adaptiveLimitSpin = nullptr;
    QCheckBox* m_adaptiveCheck = nullptr;
    QToolButton* m_hintToggle = nullptr;
};
