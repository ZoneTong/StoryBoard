#pragma once

#include "app/Settings.h"

#include <QColor>
#include <QWidget>

class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

// 设置窗的画布页：画布尺寸、外围颜色与画布存储目录；目录只读展示，
// 「Open folder」动作交回外壳打开系统目录。
class SettingsDialogCanvasPage : public QWidget {
    Q_OBJECT

  public:
    explicit SettingsDialogCanvasPage(const AppSettings& settings, QWidget* parent = nullptr);

    QGroupBox* Group() const { return m_group; }
    QPushButton* CanvasColorButton() const { return m_canvasColorButton; }

    void WriteTo(AppSettings& settings) const;
    void Reset(const AppSettings& defaults);
    void SetCanvasColor(const QColor& color);

  signals:
    void canvasColorChosen(const QColor& color);
    void openFolderRequested();

  private:
    static void RefreshColorButton(QPushButton* button, const QColor& color);

    QGroupBox* m_group = nullptr;
    QSpinBox* m_canvasWidthSpin = nullptr;
    QSpinBox* m_canvasHeightSpin = nullptr;
    QPushButton* m_canvasColorButton = nullptr;
    QColor m_canvasColor;
};
