#pragma once

#include <QList>
#include <QString>

class QAction;
class QContextMenuEvent;
class CanvasItem;
class CanvasScene;
class CanvasView;

// 右键上下文菜单：编辑模式下按命中图元组装属性 / 复制 / 平滑 / 分组 / 层级 /
// 图层 / 删除等动作，并把主窗口挂进来的动作组（如布尔运算）作为子菜单并入。
// 只负责菜单组装与派发，图元选择与场景操作仍由 CanvasScene 执行。
class CanvasContextMenuController {
  public:
    explicit CanvasContextMenuController(CanvasView* view, CanvasScene* scene);

    void AddGroup(const QString& title, const QList<QAction*>& actions);
    int GroupCount() const { return m_groups.size(); }

    void ShowMenu(QContextMenuEvent* event);

  private:
    struct ContextMenuGroup {
        QString title;
        QList<QAction*> actions;
    };

    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
    QList<ContextMenuGroup> m_groups;
};
