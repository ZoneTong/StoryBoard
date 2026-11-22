#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"

#include <QHash>
#include <QList>
#include <QRectF>
#include <QSet>
#include <utility>

namespace {

void WriteReason(GroupFailureReason* reason, GroupFailureReason value)
{
    if (reason) {
        *reason = value;
    }
}

} // namespace

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

void CanvasScene::ApplyGroupNames(const QHash<int, QString>& names)
{
    m_groupNames.clear();
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        if (it.key() > 0 && !it.value().trimmed().isEmpty()) {
            m_groupNames.insert(it.key(), it.value().trimmed());
        }
    }
}

void CanvasScene::ApplyGroupRecords(const QHash<int, QString>& names, const QHash<int, int>& parents)
{
    ApplyGroupNames(names);
    ApplyGroupParents(parents);
    update();
    emit contentChanged();
}

// ---- 组名（§7.3）----

QString CanvasScene::GroupName(int groupId) const
{
    return m_groupNames.value(groupId, QString());
}

bool CanvasScene::IsGroupNameAvailable(const QString& name, int exceptGroupId) const
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty()) {
        return false;
    }
    for (auto it = m_groupNames.constBegin(); it != m_groupNames.constEnd(); ++it) {
        if (it.key() == exceptGroupId) {
            continue;
        }
        if (it.value().compare(candidate, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

QString CanvasScene::UniqueGroupName(const QString& base) const
{
    const QString trimmed = base.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    if (IsGroupNameAvailable(trimmed)) {
        return trimmed;
    }

    // “组” → 组 1、组 2；“Hero panel” → Hero panel 2、Hero panel 3。
    const QString prefix = trimmed + QLatin1Char(' ');
    int maxSuffix = 0;
    bool exactBase = false;
    for (const QString& existing : std::as_const(m_groupNames)) {
        const QString name = existing.trimmed();
        if (name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            exactBase = true;
        }
        if (!name.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        bool ok = false;
        const int value = name.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    }
    const int next = qMax(maxSuffix + 1, exactBase ? 2 : 1);
    return prefix + QString::number(next);
}

QString CanvasScene::SuggestGroupName(const QString& base) const
{
    const QString trimmed = base.trimmed();
    if (trimmed.isEmpty()) {
        for (int n = 1;; ++n) {
            const QString candidate = QStringLiteral("组 %1").arg(n);
            if (IsGroupNameAvailable(candidate)) {
                return candidate;
            }
        }
    }
    return UniqueGroupName(trimmed);
}

// ---- 组派生查询（§7.2）----

QList<int> CanvasScene::GroupIds() const
{
    QSet<int> ids;
    for (auto it = m_groupNames.constBegin(); it != m_groupNames.constEnd(); ++it) {
        if (it.key() > 0) {
            ids.insert(it.key());
        }
    }
    for (auto it = m_groupParents.constBegin(); it != m_groupParents.constEnd(); ++it) {
        if (it.key() > 0) {
            ids.insert(it.key());
        }
        if (it.value() > 0) {
            ids.insert(it.value());
        }
    }
    for (CanvasItem* item : CanvasItems()) {
        if (item->GroupId() > 0) {
            ids.insert(item->GroupId());
        }
    }
    return QList<int>(ids.begin(), ids.end());
}

QList<int> CanvasScene::ChildGroups(int groupId) const
{
    QList<int> result;
    for (auto it = m_groupParents.constBegin(); it != m_groupParents.constEnd(); ++it) {
        if (it.value() == groupId) {
            result.append(it.key());
        }
    }
    return result;
}

QList<CanvasItem*> CanvasScene::DirectMembers(int groupId) const
{
    return ItemsInGroup(groupId);
}

QList<CanvasItem*> CanvasScene::AllLeafItems(int groupId) const
{
    return ItemsInGroupTree(groupId);
}

int CanvasScene::MemberCount(int groupId) const
{
    return ItemsInGroupTree(groupId).size();
}

QRectF CanvasScene::GroupBounds(int groupId) const
{
    QRectF bounds;
    bool first = true;
    for (CanvasItem* item : ItemsInGroupTree(groupId)) {
        const QRectF rect = item->mapRectToScene(item->FrameRect());
        if (first) {
            bounds = rect;
            first = false;
        } else {
            bounds = bounds.united(rect);
        }
    }
    return bounds;
}

bool CanvasScene::IsGroupEmpty(int groupId) const
{
    return MemberCount(groupId) == 0;
}

int CanvasScene::GroupLayerId(int groupId) const
{
    const QList<CanvasItem*> leaves = ItemsInGroupTree(groupId);
    if (leaves.isEmpty()) {
        return BackgroundLayerId;
    }
    const int layerId = leaves.constFirst()->LayerId();
    for (CanvasItem* item : leaves) {
        if (item->LayerId() != layerId) {
            return BackgroundLayerId;
        }
    }
    return layerId;
}

bool CanvasScene::IsOutermostGroup(int groupId) const
{
    return groupId > 0 && ParentGroup(groupId) <= 0;
}

bool CanvasScene::SelectionIsCompleteGroup(int* outGroupId) const
{
    const QList<CanvasItem*> selected = SelectedCanvasItems();
    if (selected.isEmpty()) {
        return false;
    }

    int outer = 0;
    for (CanvasItem* item : selected) {
        if (!item || item->GroupId() <= 0) {
            return false;
        }
        const int groupId = OutermostGroup(item->GroupId());
        if (outer == 0) {
            outer = groupId;
        } else if (groupId != outer) {
            return false;
        }
    }

    const QList<CanvasItem*> leaves = ItemsInGroupTree(outer);
    if (leaves.size() != selected.size()) {
        return false;
    }
    QSet<CanvasItem*> selectedSet;
    for (CanvasItem* item : selected) {
        selectedSet.insert(item);
    }
    for (CanvasItem* item : leaves) {
        if (!selectedSet.contains(item)) {
            return false;
        }
    }
    if (outGroupId) {
        *outGroupId = outer;
    }
    return true;
}

bool CanvasScene::HideMemberOverlay(CanvasItem* item) const
{
    if (!item || item->GroupId() <= 0) {
        return false;
    }
    int groupId = 0;
    if (!SelectionIsCompleteGroup(&groupId)) {
        return false;
    }
    return OutermostGroup(item->GroupId()) == groupId;
}

// ---- 空组清理（§7.4）----

void CanvasScene::PruneEmptyGroups()
{
    QSet<int> empty;
    for (int groupId : GroupIds()) {
        if (MemberCount(groupId) == 0) {
            empty.insert(groupId);
        }
    }
    if (empty.isEmpty()) {
        return;
    }

    // 先删最内层空子组；父组因此变空后再向上递归清理。
    bool changed = true;
    while (changed) {
        changed = false;
        for (int groupId : std::as_const(empty)) {
            if (!m_groupNames.contains(groupId) && !m_groupParents.contains(groupId)) {
                continue;
            }
            m_groupNames.remove(groupId);
            m_groupParents.remove(groupId);
            for (auto it = m_groupParents.begin(); it != m_groupParents.end();) {
                if (it.value() == groupId) {
                    it = m_groupParents.erase(it);
                } else {
                    ++it;
                }
            }
            changed = true;
        }
        for (int groupId : GroupIds()) {
            if (!empty.contains(groupId) && MemberCount(groupId) == 0) {
                empty.insert(groupId);
            }
        }
    }
    update();
    emit contentChanged();
    emit layersChanged();
}

// ---- 分组 / 解散 ----

bool CanvasScene::CanGroupSelectedItems(GroupFailureReason* reason) const
{
    if (m_mode != ToolMode::EDIT) {
        WriteReason(reason, GroupFailureReason::NOT_EDIT_MODE);
        return false;
    }

    const QList<CanvasItem*> selected = SelectedCanvasItems();
    if (selected.size() < 2) {
        WriteReason(reason, GroupFailureReason::NOT_ENOUGH_ITEMS);
        return false;
    }

    for (CanvasItem* item : selected) {
        if (!IsItemEditable(item)) {
            WriteReason(reason, GroupFailureReason::NOT_EDITABLE);
            return false;
        }
    }

    // 同层校验（§7.5）：已在组里的选中项，按整棵组树的成员图层判断。
    int layerId = -1;
    for (CanvasItem* item : selected) {
        const QList<CanvasItem*> leaves =
            item->GroupId() > 0 ? ItemsInGroupTree(OutermostGroup(item->GroupId())) : QList<CanvasItem*>{item};
        for (CanvasItem* leaf : leaves) {
            if (!IsItemEditable(leaf)) {
                WriteReason(reason, GroupFailureReason::NOT_EDITABLE);
                return false;
            }
            if (layerId < 0) {
                layerId = leaf->LayerId();
            } else if (leaf->LayerId() != layerId) {
                WriteReason(reason, GroupFailureReason::DIFFERENT_LAYERS);
                return false;
            }
        }
    }

    if (layerId < 0) {
        WriteReason(reason, GroupFailureReason::INVALID_ITEMS);
        return false;
    }
    WriteReason(reason, GroupFailureReason::NONE);
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

int CanvasScene::GroupSelectedItems(GroupFailureReason* reason)
{
    if (!CanGroupSelectedItems(reason)) {
        if (reason && *reason != GroupFailureReason::NONE) {
            emit groupOperationFailed(*reason);
        }
        return 0;
    }

    const QList<CanvasItem*> items = SelectedCanvasItems();
    const int groupId = m_nextGroupId++;
    const QString groupName = SuggestGroupName(QString());

    const QHash<int, QString> namesBefore = m_groupNames;
    const QHash<int, int> parentsBefore = m_groupParents;
    QHash<int, QString> namesAfter = namesBefore;
    QHash<int, int> parentsAfter = parentsBefore;
    namesAfter.insert(groupId, groupName);

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
        m_undoStack->push(new GroupCommand(this, items, before, after, tr("Group selected items"), parentsBefore,
                                           parentsAfter, namesBefore, namesAfter));
    } else {
        ApplyGroupIds(items, after);
        ApplyGroupRecords(namesAfter, parentsAfter);
    }
    return items.size();
}

int CanvasScene::AssignNewGroup(const QList<CanvasItem*>& items, GroupFailureReason* reason, const QString& nameBase)
{
    if (items.isEmpty()) {
        WriteReason(reason, GroupFailureReason::INVALID_ITEMS);
        return 0;
    }
    for (CanvasItem* item : items) {
        if (!item || item->scene() != this) {
            WriteReason(reason, GroupFailureReason::INVALID_ITEMS);
            return 0;
        }
    }
    if (items.size() < 2) {
        WriteReason(reason, GroupFailureReason::NOT_ENOUGH_ITEMS);
        return 0;
    }

    int layerId = -1;
    for (CanvasItem* item : items) {
        if (layerId < 0) {
            layerId = item->LayerId();
        } else if (item->LayerId() != layerId) {
            WriteReason(reason, GroupFailureReason::DIFFERENT_LAYERS);
            return 0;
        }
    }

    const int groupId = m_nextGroupId++;
    const QString groupName = SuggestGroupName(nameBase);
    QHash<int, QString> namesAfter = m_groupNames;
    namesAfter.insert(groupId, groupName);

    QList<int> groupIds;
    groupIds.reserve(items.size());
    for (CanvasItem* item : items) {
        groupIds.append(groupId);
    }
    ApplyGroupIds(items, groupIds);
    ApplyGroupNames(namesAfter);
    WriteReason(reason, GroupFailureReason::NONE);
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

    const QHash<int, QString> namesBefore = m_groupNames;
    QHash<int, QString> namesAfter = namesBefore;
    for (int groupId : std::as_const(groupIds)) {
        namesAfter.remove(groupId);
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
        m_undoStack->push(new GroupCommand(this, items, before, after, tr("Ungroup selected items"), m_groupParents,
                                           parentsAfter, namesBefore, namesAfter));
    } else {
        ApplyGroupIds(items, after);
        ApplyGroupRecords(namesAfter, parentsAfter);
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

bool CanvasScene::RenameGroupWithUndo(int groupId, const QString& name)
{
    if (groupId <= 0 || MemberCount(groupId) == 0) {
        return false;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || !IsGroupNameAvailable(trimmed, groupId)) {
        return false;
    }
    const QString before = GroupName(groupId);
    if (before.compare(trimmed, Qt::CaseInsensitive) == 0) {
        return false;
    }

    if (m_undoStack) {
        m_undoStack->push(new GroupRenameCommand(this, groupId, before, trimmed, tr("Rename group")));
    } else {
        QHash<int, QString> names = m_groupNames;
        names.insert(groupId, trimmed);
        ApplyGroupNames(names);
    }
    return true;
}

void CanvasScene::SelectGroupById(int groupId)
{
    if (groupId <= 0) {
        return;
    }
    clearSelection();
    for (CanvasItem* member : ItemsInGroupTree(groupId)) {
        if (IsItemEditable(member)) {
            member->setSelected(true);
        }
    }
}
