#pragma once

#include "app/Settings.h"

#include <QWidget>

class QComboBox;
class QGroupBox;

// 设置窗的日志页：只暴露“记录到什么详细程度”，默认错误 + 告警，
// 事件级别才记录图元的创建、销毁与属性变化。
class SettingsDialogLoggingPage : public QWidget {
    Q_OBJECT

  public:
    explicit SettingsDialogLoggingPage(const AppSettings& settings, QWidget* parent = nullptr);

    QGroupBox* Group() const { return m_group; }

    void WriteTo(AppSettings& settings) const;
    void Reset(const AppSettings& defaults);

  private:
    QGroupBox* m_group = nullptr;
    QComboBox* m_levelCombo = nullptr;
};
