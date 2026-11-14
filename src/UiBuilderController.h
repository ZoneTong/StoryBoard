#pragma once

class MainWindow;
class QToolBar;

// 界面搭建：动作、菜单、工具栏与状态栏的创建顺序、文案、objectName 与尺寸常量
// 与原 MainWindow 保持一致；QAction / 控件所有权仍归 MainWindow，本类只负责构建代码。
class UiBuilderController {
  public:
    explicit UiBuilderController(MainWindow* window);

    void buildActions();
    void buildShapeMenu();
    void buildMenus();
    void buildToolBars();
    void buildStatusBar();

  private:
    void createBrushToolbar(QToolBar* mainBar);
    void createFillToolbar(QToolBar* mainBar);

    MainWindow* m_window = nullptr;
};
