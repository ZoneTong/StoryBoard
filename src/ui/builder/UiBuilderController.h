#pragma once

#include <memory>

class MainWindow;
class UiActionFactory;
class UiMenuBuilder;
class UiToolBarBuilder;
class UiStatusBarBuilder;

// 界面搭建门面：动作定义、菜单、工具栏、状态栏分别由四个职责类完成，本类只持有
// 它们并按原顺序转发构建调用；QAction / 控件所有权仍归 MainWindow。
class UiBuilderController {
  public:
    explicit UiBuilderController(MainWindow* window);
    ~UiBuilderController();

    void BuildActions();
    void BuildShapeMenu();
    void BuildMenus();
    void BuildToolBars();
    void BuildStatusBar();

  private:
    MainWindow* m_window = nullptr;
    std::unique_ptr<UiActionFactory> m_actions;
    std::unique_ptr<UiMenuBuilder> m_menus;
    std::unique_ptr<UiToolBarBuilder> m_toolBars;
    std::unique_ptr<UiStatusBarBuilder> m_statusBar;
};
