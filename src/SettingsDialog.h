#pragma once

#include "I18n.h"
#include "Settings.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QToolButton;
class QShowEvent;

class SettingsDialog : public QDialog {
    Q_OBJECT
  public:
    explicit SettingsDialog(const AppSettings& settings, QWidget* parent = nullptr);

    AppSettings settings() const { return m_settings; }
    UiLanguage language() const;

    void setBrushColor(const QColor& color);
    void setCanvasColor(const QColor& color);

  protected:
    // 窗口显示后布局才是最终值，这里按真实内容高度补一次尺寸，避免底部说明被裁。
    void showEvent(QShowEvent* event) override;

  private:
    bool pickColor(QColor& target, const QString& title, bool allowAlpha);
    void refreshColorButton(QPushButton* button, const QColor& color);
    void resetToDefaults();
    void openCanvasesFolder();

    AppSettings m_settings;
    QPushButton* m_brushColorButton = nullptr;
    QSpinBox* m_brushWidthSpin = nullptr;
    QSpinBox* m_strokeSmoothingSpin = nullptr;
    QSpinBox* m_canvasWidthSpin = nullptr;
    QSpinBox* m_canvasHeightSpin = nullptr;
    QPushButton* m_canvasColorButton = nullptr;
    QComboBox* m_languageCombo = nullptr;
    QComboBox* m_lineArtPresetCombo = nullptr;
    bool m_applyingPreset = false;
    QSpinBox* m_lineArtLevelsSpin = nullptr;
    QSpinBox* m_lineArtPaletteMergeSpin = nullptr;
    QSpinBox* m_lineArtAdjacentMergeSpin = nullptr;
    QSpinBox* m_lineArtMinAreaSpin = nullptr;
    QSpinBox* m_lineArtAdaptiveLimitSpin = nullptr;
    QCheckBox* m_lineArtAdaptiveCheck = nullptr;
    QDoubleSpinBox* m_lineArtToleranceSpin = nullptr;
    QDoubleSpinBox* m_lineArtThicknessSpin = nullptr;
    QToolButton* m_lineArtHintToggle = nullptr;
    bool m_heightFitted = false;
};
