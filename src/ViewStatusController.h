#pragma once

#include "CanvasTypes.h"

class MainWindow;

// 视图状态与状态栏：当前工具模式切换，模式动作 / 状态栏徽章 / 各编辑动作的
// 可用与勾选状态，背景模式与右侧图元卡显隐的联动刷新，以及全景缩放。
class ViewStatusController {
  public:
    explicit ViewStatusController(MainWindow* window);

    void setToolMode(ToolMode mode);
    void updateModeUi();
    void updateActions();
    void updateStatusBar();
    void updateBackgroundModeActions();
    void fitAll();
    void updatePropertiesPanelDockVisibility();

  private:
    MainWindow* m_window = nullptr;
};
