#pragma once

class MainWindow;
class QToolBar;

// 工具栏构建：主工具栏（文件 / 图形 / 对齐 / 化形 / 平滑）与左侧模式工具栏。
// 图形区把常见图形平铺成一排，并在描线颜色后提供描线宽度下拉。
class UiToolBarBuilder {
  public:
    explicit UiToolBarBuilder(MainWindow* window);

    void buildToolBars();

  private:
    void createShapeToolbar(QToolBar* mainBar);
    void createSmoothingToolbar(QToolBar* mainBar);

    MainWindow* m_window = nullptr;
};
