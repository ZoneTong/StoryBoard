#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"

#include <QHash>
#include <QSet>
#include <utility>

QList<CanvasItem*> CanvasScene::ItemsInGroup(int groupId) const
{
    QList<CanvasItem*> result;
    if (groupId <= 0) {
        return result;
    }
    for (CanvasItem* item : CanvasItems()) {
        if (item->GroupId() == groupId) {
            result.append(item);
        }
    }
    return result;
}

int CanvasScene::ParentGroup(int groupId) const
{
    return m_groupParents.value(groupId, 0);
}

int CanvasScene::OutermostGroup(int groupId) const
{
    int current = groupId;
    int guard = 0;
    while (current > 0 && guard++ < 64) {
        const int parent = ParentGroup(current);
        if (parent <= 0) {
            break;
        }
        current = parent;
    }
    return current;
}

QList<CanvasItem*> CanvasScene::ItemsInGroupTree(int groupId) const
{
    QList<CanvasItem*> result;
    if (groupId <= 0) {
        return result;
    }
    for (CanvasItem* item : CanvasItems()) {
        if (item->GroupId() > 0 && OutermostGroup(item->GroupId()) == groupId) {
            result.append(item);
        }
    }
    return result;
}

bool CanvasScene::SelectGroupOfItem(CanvasItem* item)
{
    if (!item || item->GroupId() <= 0) {
        return false;
    }
    const int groupId = OutermostGroup(item->GroupId());
    if (groupId <= 0) {
        return false;
    }
    clearSelection();
    for (CanvasItem* member : ItemsInGroupTree(groupId)) {
        if (IsItemEditable(member)) {
            member->setSelected(true);
        }
    }
    return true;
}

void CanvasScene::ApplyGroupParents(const QHash<int, int>& parents)
{
    m_groupParents = parents;
    for (auto it = m_groupParents.constBegin(); it != m_groupParents.constEnd(); ++it) {
        m_nextGroupId = qMax(m_nextGroupId, qMax(it.key(), it.value()) + 1);
    }
}

bool CanvasScene::CanGroupSelectedItems() const
{
    if (m_mode != ToolMode::EDIT) {
        return false;
    }
    const QList<CanvasItem*> selected = SelectedCanvasItems();
    if (selected.size() < 2) {
        return false;
    }

    const int layerId = selected.constFirst()->LayerId();
    for (CanvasItem* item : selected) {
        if (!IsItemEditable(item) || item->LayerId() != layerId) {
            return false;
        }
    }
    return true;
}

bool CanvasScene::CanUngroupSelectedItems() const
{
    if (m_mode != ToolMode::EDIT) {
        return false;
    }
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (IsItemEditable(item) && item->GroupId() > 0) {
            return true;
        }
    }
    return false;
}

int CanvasScene::GroupSelectedItems()
{
    if (!CanGroupSelectedItems()) {
        return 0;
    }

    const QList<CanvasItem*> items = SelectedCanvasItems();
    const int groupId = m_nextGroupId++;
    QHash<int, int> parentsBefore = m_groupParents;
    QHash<int, int> parentsAfter = parentsBefore;
    QList<int> before;
    QList<int> after;
    before.reserve(items.size());
    after.reserve(items.size());
    for (CanvasItem* item : items) {
        before.append(item->GroupId());
        if (item->GroupId() > 0) {
            // 选中项已经在某个组里：保留它的内层归属，把那个组整体并入新组，
            // 这样旧组变成新组的子组，选中行为才按“最外层组”展开。
            parentsAfter.insert(OutermostGroup(item->GroupId()), groupId);
            after.append(item->GroupId());
        } else {
            after.append(groupId);
        }
    }

    if (m_undoStack) {
        m_undoStack->push(
            new GroupCommand(this, items, before, after, tr("Group selected items"), parentsBefore, parentsAfter));
    } else {
        ApplyGroupIds(items, after);
        ApplyGroupParents(parentsAfter);
    }
    return items.size();
}

int CanvasScene::AssignNewGroup(const QList<CanvasItem*>& items)
{
    if (items.size() < 2) {
        return 0;
    }
    const int groupId = m_nextGroupId++;
    QList<int> groupIds;
    groupIds.reserve(items.size());
    for (CanvasItem* item : items) {
        if (!item) {
            return 0;
        }
        groupIds.append(groupId);
    }
    ApplyGroupIds(items, groupIds);
    return groupId;
}

int CanvasScene::UngroupSelectedItems()
{
    if (!CanUngroupSelectedItems()) {
        return 0;
    }

    QList<CanvasItem*> items;
    QSet<CanvasItem*> seen;
    QSet<int> groupIds;
    for (CanvasItem* selected : SelectedCanvasItems()) {
        if (IsItemEditable(selected) && selected->GroupId() > 0) {
            groupIds.insert(OutermostGroup(selected->GroupId()));
        }
    }
    QHash<int, int> parentsAfter = m_groupParents;
    for (int groupId : std::as_const(groupIds)) {
        for (CanvasItem* member : ItemsInGroupTree(groupId)) {
            if (!seen.contains(member)) {
                seen.insert(member);
                items.append(member);
            }
        }
        parentsAfter.remove(groupId);
        // 子组随父组一起解散，父子关系全部清掉，避免留下悬空的父指针。
        for (auto it = parentsAfter.begin(); it != parentsAfter.end();) {
            if (it.value() == groupId) {
                it = parentsAfter.erase(it);
            } else {
                ++it;
            }
        }
    }
    if (items.isEmpty()) {
        return 0;
    }

    QList<int> before;
    QList<int> after;
    before.reserve(items.size());
    after.reserve(items.size());
    for (CanvasItem* item : items) {
        before.append(item->GroupId());
        after.append(0);
    }

    if (m_undoStack) {
        m_undoStack->push(
            new GroupCommand(this, items, before, after, tr("Ungroup selected items"), m_groupParents, parentsAfter));
    } else {
        ApplyGroupIds(items, after);
        ApplyGroupParents(parentsAfter);
    }
    return items.size();
}

void CanvasScene::ApplyGroupIds(const QList<CanvasItem*>& items, const QList<int>& groupIds)
{
    if (items.size() != groupIds.size()) {
        return;
    }
    for (int i = 0; i < items.size(); ++i) {
        if (items.at(i)) {
            items.at(i)->SetGroupId(groupIds.at(i));
        }
    }
    update();
    emit contentChanged();
}
