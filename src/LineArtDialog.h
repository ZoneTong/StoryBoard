#pragma once

#include "LineArtConverter.h"
#include "Settings.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QSpinBox;

// 选完图片后弹出的线稿化参数对话框：档位一键切换整组参数，手改任一项就回到“自定义”。
// 只有点“确定”才会把这里的参数交给转换器，取消则什么都不生成。
class LineArtDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LineArtDialog(const AppSettings& settings, QWidget* parent = nullptr);

    LineArtConverter::Options options() const;
    void applyToSettings(AppSettings& settings) const;

private:
    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_levelsSpin = nullptr;
    QSpinBox* m_paletteMergeSpin = nullptr;
    QSpinBox* m_adjacentMergeSpin = nullptr;
    QSpinBox* m_minAreaSpin = nullptr;
    QSpinBox* m_adaptiveLimitSpin = nullptr;
    QCheckBox* m_adaptiveCheck = nullptr;
    bool m_applyingPreset = false;
};
