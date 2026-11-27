#pragma once

#include "app/I18n.h"
#include "app/Settings.h"

#include <QDialog>

class QComboBox;
class QShowEvent;
class SettingsDialogBrushPage;
class SettingsDialogCanvasPage;
class SettingsDialogLineArtPage;
class SettingsDialogLoggingPage;

// 画板默认设置对话框。外壳只负责标题、语言选择、整体滚动布局与确定 / 取消 /
// 恢复默认；画笔默认值、画布、图片线稿化参数分别由对应页面类构建与收集。
class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit SettingsDialog(const AppSettings& settings, QWidget* parent = nullptr);

    AppSettings Settings() const { return m_settings; }
    UiLanguage Language() const;

    void SetBrushColor(const QColor& color);
    void SetCanvasColor(const QColor& color);

  protected:
    // 窗口显示后布局才是最终值，这里按真实内容高度补一次尺寸，避免底部说明被裁。
    void showEvent(QShowEvent* event) override;

  private:
    void ResetToDefaults();
    void OpenCanvasesFolder();

    AppSettings m_settings;
    QComboBox* m_languageCombo = nullptr;
    SettingsDialogBrushPage* m_brushPage = nullptr;
    SettingsDialogCanvasPage* m_canvasPage = nullptr;
    SettingsDialogLineArtPage* m_lineArtPage = nullptr;
    SettingsDialogLoggingPage* m_loggingPage = nullptr;
    bool m_heightFitted = false;
};
