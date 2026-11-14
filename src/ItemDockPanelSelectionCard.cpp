#include "ItemDockPanelSelectionCard.h"

#include <QAction>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace {

constexpr int kButtonSize = 26;
constexpr int kIconSize = 14;
constexpr int kAlignButtonCount = 8;
constexpr int kBoolButtonCount = 5;

// 每一行都是独立容器，行内按钮平铺；行容器负责整行的显隐，避免逐排撑开空行。
QWidget* makeRow(QWidget* parent, QHBoxLayout** layout)
{
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);
    *layout = rowLayout;
    return row;
}

} // namespace

ItemDockPanelSelectionCard::ItemDockPanelSelectionCard(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(3);

    QHBoxLayout* basicLayout = nullptr;
    m_basicRow = makeRow(this, &basicLayout);
    QHBoxLayout* alignLayout = nullptr;
    m_alignRow = makeRow(this, &alignLayout);
    QHBoxLayout* morphLayout = nullptr;
    m_morphRow = makeRow(this, &morphLayout);
    QHBoxLayout* groupLayout = nullptr;
    m_groupRow = makeRow(this, &groupLayout);

    m_copyButton = makeButton(QStringLiteral("ItemCopyButton"));
    basicLayout->addWidget(m_copyButton);
    m_deleteButton = makeButton(QStringLiteral("ItemDeleteButton"));
    basicLayout->addWidget(m_deleteButton);
    m_rotateCwButton = makeButton(QStringLiteral("ItemRotateCwButton"));
    basicLayout->addWidget(m_rotateCwButton);
    m_rotateCcwButton = makeButton(QStringLiteral("ItemRotateCcwButton"));
    basicLayout->addWidget(m_rotateCcwButton);
    basicLayout->addStretch(1);

    // 对齐 8 项与化形 5 项各占一行：多选时直接平铺，不再收进下拉菜单。
    for (int i = 0; i < kAlignButtonCount; ++i) {
        auto* button = makeButton(QStringLiteral("ItemAlignRowButton"));
        m_alignButtons.append(button);
        alignLayout->addWidget(button);
    }
    alignLayout->addStretch(1);
    for (int i = 0; i < kBoolButtonCount; ++i) {
        auto* button = makeButton(QStringLiteral("ItemMorphButton"));
        m_boolButtons.append(button);
        morphLayout->addWidget(button);
    }
    morphLayout->addStretch(1);

    m_groupButton = makeButton(QStringLiteral("ItemGroupButton"));
    groupLayout->addWidget(m_groupButton);
    m_ungroupButton = makeButton(QStringLiteral("ItemUngroupButton"));
    groupLayout->addWidget(m_ungroupButton);
    groupLayout->addStretch(1);

    outer->addWidget(m_basicRow);
    outer->addWidget(m_alignRow);
    outer->addWidget(m_morphRow);
    outer->addWidget(m_groupRow);

    hide();
}

QToolButton* ItemDockPanelSelectionCard::makeButton(const QString& objectName)
{
    auto* button = new QToolButton(this);
    button->setObjectName(objectName);
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->hide();
    return button;
}

void ItemDockPanelSelectionCard::bindAction(QToolButton* button, QAction* action)
{
    if (!button) {
        return;
    }
    // 不再依赖 setDefaultAction 的隐式转发：按钮显示后直接点击必须能触发动作。
    button->setDefaultAction(nullptr);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIcon(action ? action->icon() : QIcon());
    button->setToolTip(action ? action->text() : QString());
    button->setAccessibleName(action ? action->text() : QString());
    button->setProperty("cardActionText", action ? action->text() : QString());
    button->setEnabled(action && action->isEnabled());
    if (action) {
        QObject::connect(button, &QToolButton::clicked, action, &QAction::trigger, Qt::UniqueConnection);
    }
}

QAction* ItemDockPanelSelectionCard::actionForButton(QToolButton* button) const
{
    if (!button) {
        return nullptr;
    }
    if (button == m_copyButton) {
        return m_copyAction;
    }
    if (button == m_deleteButton) {
        return m_deleteAction;
    }
    if (button == m_rotateCwButton) {
        return m_rotateCwAction;
    }
    if (button == m_rotateCcwButton) {
        return m_rotateCcwAction;
    }
    if (button == m_groupButton) {
        return m_groupAction;
    }
    if (button == m_ungroupButton) {
        return m_ungroupAction;
    }
    const int alignIndex = m_alignButtons.indexOf(button);
    if (alignIndex >= 0 && alignIndex < m_alignActions.size()) {
        return m_alignActions.at(alignIndex);
    }
    const int boolIndex = m_boolButtons.indexOf(button);
    if (boolIndex >= 0 && boolIndex < m_boolActions.size()) {
        return m_boolActions.at(boolIndex);
    }
    return nullptr;
}

void ItemDockPanelSelectionCard::setSelectionActions(const QList<QAction*>& alignActions,
                                                     const QList<QAction*>& boolActions, QAction* copyAction,
                                                     QAction* removeAction, QAction* rotateCwAction,
                                                     QAction* rotateCcwAction, QAction* groupAction,
                                                     QAction* ungroupAction)
{
    m_alignActions = alignActions;
    m_boolActions = boolActions;
    for (int i = 0; i < m_alignButtons.size() && i < alignActions.size(); ++i) {
        bindAction(m_alignButtons.at(i), alignActions.at(i));
    }
    for (int i = 0; i < m_boolButtons.size() && i < boolActions.size(); ++i) {
        bindAction(m_boolButtons.at(i), boolActions.at(i));
    }

    m_copyAction = copyAction;
    m_deleteAction = removeAction;
    m_rotateCwAction = rotateCwAction;
    m_rotateCcwAction = rotateCcwAction;
    m_groupAction = groupAction;
    m_ungroupAction = ungroupAction;
    bindAction(m_copyButton, copyAction);
    bindAction(m_deleteButton, removeAction);
    bindAction(m_rotateCwButton, rotateCwAction);
    bindAction(m_rotateCcwButton, rotateCcwAction);
    bindAction(m_groupButton, groupAction);
    bindAction(m_ungroupButton, ungroupAction);

    refreshVisibility();
}

void ItemDockPanelSelectionCard::setSelectionState(bool hasSelection, bool groupUseful)
{
    m_hasSelection = hasSelection;
    m_groupUseful = groupUseful;
    refreshVisibility();
}

void ItemDockPanelSelectionCard::refreshVisibility()
{
    // 动作不可用就整颗按钮隐藏：卡片只列出当前选中真正能做的操作。
    auto usable = [this](QToolButton* button) {
        QAction* action = actionForButton(button);
        const bool enabled = action && action->isEnabled();
        button->setEnabled(enabled);
        button->setVisible(enabled);
        return enabled;
    };

    bool anyBasic = false;
    anyBasic = usable(m_copyButton) || anyBasic;
    anyBasic = usable(m_deleteButton) || anyBasic;
    anyBasic = usable(m_rotateCwButton) || anyBasic;
    anyBasic = usable(m_rotateCcwButton) || anyBasic;

    bool anyAlign = false;
    for (QToolButton* button : std::as_const(m_alignButtons)) {
        anyAlign = usable(button) || anyAlign;
    }
    bool anyMorph = false;
    for (QToolButton* button : std::as_const(m_boolButtons)) {
        anyMorph = usable(button) || anyMorph;
    }

    const bool groupVisible = m_groupUseful && usable(m_groupButton);
    const bool ungroupVisible = usable(m_ungroupButton);
    m_groupButton->setVisible(groupVisible);
    m_ungroupButton->setVisible(ungroupVisible);

    m_basicRow->setVisible(anyBasic);
    m_alignRow->setVisible(anyAlign);
    m_morphRow->setVisible(anyMorph);
    m_groupRow->setVisible(groupVisible || ungroupVisible);
    setVisible(m_hasSelection && (anyBasic || anyAlign || anyMorph || groupVisible || ungroupVisible));
}
