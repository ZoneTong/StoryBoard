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

void ItemDockPanelSelectionCard::setSelectionActions(const QList<QAction*>& alignActions,
                                                     const QList<QAction*>& boolActions, QAction* copyAction,
                                                     QAction* removeAction, QAction* rotateCwAction,
                                                     QAction* rotateCcwAction, QAction* groupAction,
                                                     QAction* ungroupAction)
{
    for (int i = 0; i < m_alignButtons.size() && i < alignActions.size(); ++i) {
        m_alignButtons.at(i)->setDefaultAction(alignActions.at(i));
    }
    for (int i = 0; i < m_boolButtons.size() && i < boolActions.size(); ++i) {
        m_boolButtons.at(i)->setDefaultAction(boolActions.at(i));
    }

    m_copyAction = copyAction;
    m_deleteAction = removeAction;
    m_rotateCwAction = rotateCwAction;
    m_rotateCcwAction = rotateCcwAction;
    m_groupAction = groupAction;
    m_ungroupAction = ungroupAction;
    if (copyAction) {
        m_copyButton->setDefaultAction(copyAction);
    }
    if (removeAction) {
        m_deleteButton->setDefaultAction(removeAction);
    }
    if (rotateCwAction) {
        m_rotateCwButton->setDefaultAction(rotateCwAction);
    }
    if (rotateCcwAction) {
        m_rotateCcwButton->setDefaultAction(rotateCcwAction);
    }
    if (groupAction) {
        m_groupButton->setDefaultAction(groupAction);
    }
    if (ungroupAction) {
        m_ungroupButton->setDefaultAction(ungroupAction);
    }

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
    auto usable = [](QToolButton* button) {
        QAction* action = button->defaultAction();
        const bool enabled = action && action->isEnabled();
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

    const bool groupVisible = m_groupUseful && m_groupAction && m_groupAction->isEnabled();
    const bool ungroupVisible = m_ungroupAction && m_ungroupAction->isEnabled();
    m_groupButton->setVisible(groupVisible);
    m_ungroupButton->setVisible(ungroupVisible);

    m_basicRow->setVisible(anyBasic);
    m_alignRow->setVisible(anyAlign);
    m_morphRow->setVisible(anyMorph);
    m_groupRow->setVisible(groupVisible || ungroupVisible);
    setVisible(m_hasSelection && (anyBasic || anyAlign || anyMorph || groupVisible || ungroupVisible));
}
