#pragma once

#include <QString>

class MainWindow;

// 去掉提示文字末尾的键位括号（如 “ (Ctrl+N)” / “（Ctrl+N）”），返回纯描述基串。
// makeAction 入库与 ShortcutController::refreshShortcutTips 重拼共用这一份正则，
// 避免两处漂移，也保证基串永远是「不带键位的纯描述」。
QString stripShortcutTail(const QString& tip);

// 动作定义：创建主窗口全部 QAction / QActionGroup，连接触发信号。
// 动作对象所有权仍归 MainWindow，本类只负责创建、文案、键位与信号接线。
class UiActionFactory {
  public:
    explicit UiActionFactory(MainWindow* window);

    void build();

  private:
    MainWindow* m_window = nullptr;
};
