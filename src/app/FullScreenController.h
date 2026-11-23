#pragma once

#include <QList>
#include <QRect>
#include <memory>

class FullScreenBar;
class MainWindow;
class QAction;
class QDockWidget;
class QToolBar;

// 全屏模式的状态机：进入时收起菜单栏、状态栏、主工具栏与模式卡，只留画布与浮层，
// 退出时按快照精确还原；全屏期间把带键位的动作重新挂到画布视图上，
// 否则隐藏的菜单栏 / 工具栏会让它们的快捷键静默失效。
class FullScreenController {
  public:
    explicit FullScreenController(MainWindow* window);
    ~FullScreenController();

    void Toggle();
    // 关窗前调用：只有确实在全屏时才退出，避免误触发进入全屏。
    void ExitIfActive();
    bool IsActive() const { return m_active; }
    // 窗口被窗口管理器踢出全屏时兜底：只还原界面，不再重复 showNormal。
    void HandleWindowStateChange();
    // 主窗口尺寸变化时把悬浮工具栏重新摆到顶部居中。
    void HandleWindowEvent(class QObject* watched, class QEvent* event);

  private:
    void Enter();
    void Exit();
    void RememberChrome();
    void RestoreChrome(bool restoreWindowGeometry);
    // 全屏期保活：把菜单、工具栏与模式组里的键位动作临时挂到画布视图。
    void RegisterActionShortcuts();
    void UnregisterActionShortcuts();

    QToolBar* MainToolBar() const;
    QDockWidget* ViewModeDock() const;
    void PlaceBar();

    MainWindow* m_window = nullptr;
    bool m_active = false;
    bool m_chromeValid = false;
    bool m_menuBarVisible = true;
    bool m_statusBarVisible = true;
    bool m_mainToolBarVisible = true;
    bool m_viewModeDockVisible = true;
    bool m_wasMaximized = false;
    QRect m_geometry;
    QList<QAction*> m_extraShortcutActions;
    std::unique_ptr<FullScreenBar> m_bar;
};
