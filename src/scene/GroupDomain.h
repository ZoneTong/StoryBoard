#pragma once

#include "scene/GroupState.h"

#include <QHash>
#include <QList>
#include <QString>

// 组记录的唯一持有者：组名、父子关系与下一个组号。
// 不持有图元生命周期，也不碰撤销栈；CanvasScene 只做 facade 与图元归属写入。
class GroupDomain {
  public:
    void Reset();
    // 分配新组号并推进计数器。
    int NextId();
    // 载入 / 复制时看到已有组号，只推进计数器，不创建记录。
    void ObserveId(int groupId);

    int Parent(int groupId) const;
    void SetParent(int childGroupId, int parentGroupId);
    QHash<int, int> Parents() const { return m_parents; }
    void ApplyParents(const QHash<int, int>& parents);
    QList<int> ChildGroups(int groupId) const;

    QString Name(int groupId) const;
    void SetName(int groupId, const QString& name);
    QHash<int, QString> Names() const { return m_names; }
    void ApplyNames(const QHash<int, QString>& names);
    bool IsNameAvailable(const QString& name, int exceptGroupId = 0) const;
    QString UniqueName(const QString& base) const;

    QList<int> RecordedIds() const;
    bool HasRecord(int groupId) const;
    void RemoveRecord(int groupId);

    GroupState Snapshot() const;
    void ApplyRecords(const GroupState& groups);

  private:
    int m_nextGroupId = 1;
    QHash<int, int> m_parents;
    QHash<int, QString> m_names;
};
