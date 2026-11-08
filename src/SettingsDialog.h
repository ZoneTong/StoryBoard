#pragma once

#include <QDialog>

#include "Settings.h"
#include "I18n.h"

class QCheckBox;
class QPushButton;
class QSpinBox;
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
    QSpinBox* m_boardWidthSpin = nullptr;
    QSpinBox* m_boardHeightSpin = nullptr;
    QCheckBox* m_gridCheck = nullptr;
    QPushButton* m_canvasColorButton = nullptr;
    QComboBox* m_languageCombo = nullptr;
};
