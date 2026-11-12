#pragma once

#include "LineArtConverter.h"
#include "Settings.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;

// 选完图片后弹出的线稿化参数对话框：档位一键切换整组参数，手改任一项就回到“自定义”。
// 只有点“确定”才会把这里的参数交给转换器，取消则什么都不生成。
// 参数默认只在内存里生效（本次进程有效），点“保存到配置文件”才覆盖 StoryBoard.ini。
class LineArtDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LineArtDialog(const AppSettings& settings, QWidget* parent = nullptr);

    LineArtConverter::Options options() const;
    void applyToSettings(AppSettings& settings) const;
    // 用户是否点了“保存到配置文件”；未点则调用方只更新内存，不写文件。
    bool saveRequested() const { return m_saveToConfig; }

private:
    QPushButton* m_saveToConfigButton = nullptr;
    QComboBox* m_presetCombo = nullptr;
    QSpinBox* m_levelsSpin = nullptr;
    QSpinBox* m_paletteMergeSpin = nullptr;
    QSpinBox* m_adjacentMergeSpin = nullptr;
    QSpinBox* m_minAreaSpin = nullptr;
    QSpinBox* m_adaptiveLimitSpin = nullptr;
    QCheckBox* m_adaptiveCheck = nullptr;
    QDoubleSpinBox* m_toleranceSpin = nullptr;
    QDoubleSpinBox* m_thicknessSpin = nullptr;
    bool m_applyingPreset = false;
    bool m_saveToConfig = false;
};
