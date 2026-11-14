#pragma once

class MainWindow;

// 应用级动作：设置对话框与落盘、关于窗口，以及整棵界面树的语言切换刷新。
// 画笔 / 填充等样式状态的具体改写仍由 StyleController 链路负责，这里只触发刷新。
class MainWindowAppController {
  public:
    explicit MainWindowAppController(MainWindow* window);

    void applyUiLanguage();
    void showSettings();
    void showAbout();
    // 退出前把窗口几何、停靠顺序、画笔与画布参数写回配置；由 closeEvent 调用。
    void persistBeforeExit();

  private:
    MainWindow* m_window = nullptr;
};
