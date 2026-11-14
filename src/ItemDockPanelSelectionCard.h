#pragma once

#include <QList>
#include <QWidget>

class QAction;
class QMenu;
class QToolButton;

// 多选时出现在常见图形下方的对齐 / 合并形状入口：菜单动作复用主窗口同一批 QAction，
// 卡片只负责按钮、菜单容器、多选显隐与按钮启用状态刷新。
class ItemDockPanelSelectionCard : public QWidget {
    Q_OBJECT

  public:
    explicit ItemDockPanelSelectionCard(QWidget* parent = nullptr);

    // 用主窗口的对齐 / 合并形状动作填充两个下拉菜单，动作本身的触发与启用由主窗口维护。
    void setSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions);
    // 选中数 >= 2 时显示整张卡片；同时按动作启用状态刷新两个按钮的可用态。
    void setMultiSelection(bool multi);

  private:
    void refreshButtonEnabled();

    QToolButton* m_alignButton = nullptr;
    QToolButton* m_mergeButton = nullptr;
    QMenu* m_alignMenu = nullptr;
    QMenu* m_mergeMenu = nullptr;
};
