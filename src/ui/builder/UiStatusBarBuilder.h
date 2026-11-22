#pragma once

class MainWindow;

// 状态栏构建：提示标签与模式 / 缩放 / 数量三个常驻徽章的创建与挂载。
class UiStatusBarBuilder {
  public:
    explicit UiStatusBarBuilder(MainWindow* window);

    void BuildStatusBar();

  private:
    MainWindow* m_window = nullptr;
};
