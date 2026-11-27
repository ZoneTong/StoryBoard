#pragma once

#include <QList>
#include <QString>

class CanvasSession;
class MainWindow;
class QTabWidget;

// 中央区的画布标签：持有 QTabWidget 与全部 CanvasSession，负责新建 / 激活 /
// 前后切换 / 关页后的收尾，以及与「当前会话」指针的重绑衔接。
// 文件读写、未保存询问仍归 DocumentController，这里只提供会话层面的原语。
class CanvasTabsController {
  public:
    explicit CanvasTabsController(MainWindow* window);

    QTabWidget* Tabs() const { return m_tabs; }

    // 首份文档：接管 MainWindow 已构造好的场景 / 视图 / 撤销栈，并挂进标签。
    void AdoptSession(CanvasSession* session);
    // 新开一份空白画布；activate 为真时立即切过去。
    CanvasSession* CreateSession(bool activate = true);

    QList<CanvasSession*> Sessions() const { return m_sessions; }
    int Count() const { return m_sessions.size(); }
    CanvasSession* Current() const { return m_current; }
    CanvasSession* SessionAt(int index) const;
    int IndexOf(CanvasSession* session) const;
    CanvasSession* FindByPath(const QString& path) const;

    void SetCurrent(CanvasSession* session);
    void ActivateIndex(int index);
    void Next();
    void Previous();
    // 1 起算：Ctrl+1 到第 1 个画布；超出数量时不动作。
    void SwitchToNumber(int oneBasedNumber);

    // 移除会话并销毁它；若移除的是最后一页，自动补一份空白画布。
    // 调用方（DocumentController）负责先做完未保存询问。
    void RemoveSession(CanvasSession* session);
    // 窗口析构专用：摘掉全部页面并销毁会话，不补空白页。
    void ClearAll();

    // 标签文字：画布名，未保存时前缀「• 」；tooltip 放完整路径。
    void UpdateTabLabel(CanvasSession* session);
    void RefreshLabels();
    // 全屏期隐藏标签栏（页面栈里非当前页本就不可见）。
    void SetTabBarVisible(bool visible);

  private:
    MainWindow* m_window = nullptr;
    QTabWidget* m_tabs = nullptr;
    QList<CanvasSession*> m_sessions;
    CanvasSession* m_current = nullptr;
    int m_untitledCount = 0;
    int m_canvasCount = 0;
    // 切换会话时的重入保护：QTabWidget::currentChanged 会回调到这里。
    bool m_switching = false;
};
