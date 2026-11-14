#pragma once

class MainWindow;
class QToolBar;

// 工具栏构建：主工具栏（文件 / 编辑 / 对齐 / 合并 / 填充 / 画笔 / 视图 / 背景 /
// 设置）与左侧模式工具栏，以及填充区、画笔区两个工具条子部件。
class UiToolBarBuilder {
  public:
    explicit UiToolBarBuilder(MainWindow* window);

    void buildToolBars();

  private:
    void createBrushToolbar(QToolBar* mainBar);
    void createFillToolbar(QToolBar* mainBar);

    MainWindow* m_window = nullptr;
};
