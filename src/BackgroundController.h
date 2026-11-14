#pragma once

class MainWindow;

// 画布背景：替换背景图、设置背景色、清除背景，以及背景模式（拉伸 / 适应 / 平铺）
// 动作的可用与勾选状态。背景数据本身仍是 CanvasScene 的撤销记录。
class BackgroundController {
  public:
    explicit BackgroundController(MainWindow* window);

    void replaceBackgroundImage();
    void setBackgroundColor();
    void clearBackground();
    void updateModeActions();

  private:
    MainWindow* m_window = nullptr;
};
