#pragma once

#include <QDialog>

#include "Settings.h"
#include "I18n.h"

class QCheckBox;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QComboBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings& settings, QWidget* parent = nullptr);

    AppSettings settings() const { return m_settings; }
    UiLanguage language() const;

    void setBrushColor(const QColor& color);
    void setCanvasColor(const QColor& color);

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
    QCheckBox* m_gridCheck = nullptr;
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
};
