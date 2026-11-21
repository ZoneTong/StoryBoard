#pragma once

#include "CanvasTypes.h"

class MainWindow;

// 视图状态与状态栏：当前工具模式切换，模式动作 / 状态栏徽章 / 各编辑动作的
// 可用与勾选状态，背景模式与右侧图元卡显隐的联动刷新，以及全景缩放。
class ViewStatusController {
  public:
    explicit ViewStatusController(MainWindow* window);

    void SetToolMode(ToolMode mode);
    void UpdateModeUi();
    void UpdateActions();
    void UpdateStatusBar();
    void UpdateBackgroundModeActions();
    void FitAll();
    void UpdatePropertiesPanelDockVisibility();

  private:
    MainWindow* m_window = nullptr;
};
