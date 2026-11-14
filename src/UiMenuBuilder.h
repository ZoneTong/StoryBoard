#pragma once

class MainWindow;

// 菜单构建：常见图形菜单（含自定义形状子菜单）与一级菜单（文件 / 视图 / 编辑 /
// 插入 / 画笔 / 帮助）的创建顺序、objectName 与动作挂载，和原实现保持一致。
class UiMenuBuilder {
  public:
    explicit UiMenuBuilder(MainWindow* window);

    void buildShapeMenu();
    void buildMenus();

  private:
    MainWindow* m_window = nullptr;
};
