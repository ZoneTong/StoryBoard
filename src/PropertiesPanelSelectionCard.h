#pragma once

#include <QList>
#include <QWidget>

class QAction;
class QToolButton;

// 属性编辑面板的选中操作栏：单选时提供复制 / 删除 / 顺、逆时针旋转；多选时把对齐 8 项与
// 合并图元 6 项直接展开成图标按钮行，再按当前选择能否组合 / 取消组合显示组操作按钮。
// 按钮全部复用主窗口 QAction，卡片只负责图标按钮、逐行显隐与整卡可见性。
class PropertiesPanelSelectionCard : public QWidget {
    Q_OBJECT

  public:
    explicit PropertiesPanelSelectionCard(QWidget* parent = nullptr);

    // 用主窗口已有的对齐 / 合并图元 / 编辑动作填充按钮，触发与启用状态由主窗口维护。
    void SetSelectionActions(const QList<QAction*>& alignActions, const QList<QAction*>& boolActions,
                             QAction* copyAction, QAction* removeAction, QAction* rotateCwAction,
                             QAction* rotateCcwAction, QAction* groupAction, QAction* ungroupAction);
    // 笔画平滑与图元属性窗：动作可用时按钮才出现，和菜单 / 工具栏共用同一批 QAction。
    void SetItemActions(QAction* smoothAction, QAction* propertiesAction);
    // 有选中才可能显示整卡；groupUseful 为 false（选中的正好是一整个组）时不再提供“组合”。
    void SetSelectionState(bool hasSelection, bool groupUseful);

  private:
    QToolButton* MakeButton(const QString& objectName);
    void BindAction(QToolButton* button, QAction* action);
    QAction* ActionForButton(QToolButton* button) const;
    void RefreshVisibility();

    QWidget* m_basicRow = nullptr;
    QWidget* m_alignRow = nullptr;
    QWidget* m_morphRow = nullptr;
    QWidget* m_groupRow = nullptr;

    QList<QToolButton*> m_alignButtons;
    QList<QToolButton*> m_boolButtons;
    QList<QAction*> m_alignActions;
    QList<QAction*> m_boolActions;
    QToolButton* m_copyButton = nullptr;
    QToolButton* m_deleteButton = nullptr;
    QToolButton* m_rotateCwButton = nullptr;
    QToolButton* m_rotateCcwButton = nullptr;
    QToolButton* m_smoothButton = nullptr;
    QToolButton* m_propertiesButton = nullptr;
    QToolButton* m_groupButton = nullptr;
    QToolButton* m_ungroupButton = nullptr;

    QAction* m_copyAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_rotateCwAction = nullptr;
    QAction* m_rotateCcwAction = nullptr;
    QAction* m_smoothAction = nullptr;
    QAction* m_propertiesAction = nullptr;
    QAction* m_groupAction = nullptr;
    QAction* m_ungroupAction = nullptr;

    bool m_hasSelection = false;
    bool m_groupUseful = false;
};
