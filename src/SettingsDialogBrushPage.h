#pragma once

#include "Settings.h"

#include <QColor>
#include <QWidget>

class QGroupBox;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;

// 设置窗的画笔默认值页：默认颜色、默认宽度与自动平滑容差；颜色按钮在页内
// 打开颜色对话框，选好后把颜色交回外壳写进设置。
class SettingsDialogBrushPage : public QWidget {
    Q_OBJECT

  public:
    explicit SettingsDialogBrushPage(const AppSettings& settings, QWidget* parent = nullptr);

    QGroupBox* group() const { return m_group; }
    QPushButton* brushColorButton() const { return m_brushColorButton; }

    void writeTo(AppSettings& settings) const;
    void reset(const AppSettings& defaults);
    void setBrushColor(const QColor& color);

  signals:
    void brushColorChosen(const QColor& color);

  private:
    static void refreshColorButton(QPushButton* button, const QColor& color);

    QGroupBox* m_group = nullptr;
    QPushButton* m_brushColorButton = nullptr;
    QSpinBox* m_brushWidthSpin = nullptr;
    QDoubleSpinBox* m_smoothingSpin = nullptr;
    QColor m_brushColor;
};
