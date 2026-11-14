#pragma once

class MainWindow;

// 动作定义：创建主窗口全部 QAction / QActionGroup，连接触发信号。
// 动作对象所有权仍归 MainWindow，本类只负责创建、文案、键位与信号接线。
class UiActionFactory {
  public:
    explicit UiActionFactory(MainWindow* window);

    void build();

  private:
    MainWindow* m_window = nullptr;
};
