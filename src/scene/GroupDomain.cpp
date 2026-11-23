#include "scene/GroupDomain.h"

#include <QSet>
#include <algorithm>

void GroupDomain::Reset()
{
    m_nextGroupId = 1;
    m_parents.clear();
    m_names.clear();
}

int GroupDomain::NextId()
{
    return m_nextGroupId++;
}

void GroupDomain::ObserveId(int groupId)
{
    if (groupId > 0) {
        m_nextGroupId = qMax(m_nextGroupId, groupId + 1);
    }
}

int GroupDomain::Parent(int groupId) const
{
    return m_parents.value(groupId, 0);
}

void GroupDomain::SetParent(int childGroupId, int parentGroupId)
{
    if (childGroupId <= 0) {
        return;
    }
    if (parentGroupId > 0) {
        m_parents.insert(childGroupId, parentGroupId);
        ObserveId(parentGroupId);
    } else {
        m_parents.remove(childGroupId);
    }
    ObserveId(childGroupId);
}

void GroupDomain::ApplyParents(const QHash<int, int>& parents)
{
    m_parents.clear();
    for (auto it = parents.constBegin(); it != parents.constEnd(); ++it) {
        if (it.key() <= 0) {
            continue;
        }
        m_parents.insert(it.key(), it.value());
        ObserveId(it.key());
        ObserveId(it.value());
    }
}

QList<int> GroupDomain::ChildGroups(int groupId) const
{
    QList<int> result;
    for (auto it = m_parents.constBegin(); it != m_parents.constEnd(); ++it) {
        if (it.value() == groupId) {
            result.append(it.key());
        }
    }
    return result;
}

QString GroupDomain::Name(int groupId) const
{
    return m_names.value(groupId, QString());
}

void GroupDomain::SetName(int groupId, const QString& name)
{
    if (groupId <= 0) {
        return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        m_names.remove(groupId);
    } else {
        m_names.insert(groupId, trimmed);
    }
    ObserveId(groupId);
}

void GroupDomain::ApplyNames(const QHash<int, QString>& names)
{
    m_names.clear();
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        if (it.key() <= 0 || it.value().trimmed().isEmpty()) {
            continue;
        }
        m_names.insert(it.key(), it.value().trimmed());
        ObserveId(it.key());
    }
}

bool GroupDomain::IsNameAvailable(const QString& name, int exceptGroupId) const
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty()) {
        return false;
    }
    for (auto it = m_names.constBegin(); it != m_names.constEnd(); ++it) {
        if (it.key() == exceptGroupId) {
            continue;
        }
        if (it.value().compare(candidate, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

QString GroupDomain::UniqueName(const QString& base) const
{
    const QString trimmed = base.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    if (IsNameAvailable(trimmed)) {
        return trimmed;
    }

    // “组” → 组 1、组 2；“Hero panel” → Hero panel 2、Hero panel 3。
    const QString prefix = trimmed + QLatin1Char(' ');
    int maxSuffix = 0;
    bool exactBase = false;
    for (const QString& existing : m_names) {
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

QList<int> GroupDomain::RecordedIds() const
{
    QSet<int> ids;
    for (auto it = m_names.constBegin(); it != m_names.constEnd(); ++it) {
        if (it.key() > 0) {
            ids.insert(it.key());
        }
    }
    for (auto it = m_parents.constBegin(); it != m_parents.constEnd(); ++it) {
        if (it.key() > 0) {
            ids.insert(it.key());
        }
        if (it.value() > 0) {
            ids.insert(it.value());
        }
    }
    QList<int> result(ids.begin(), ids.end());
    std::sort(result.begin(), result.end());
    return result;
}

bool GroupDomain::HasRecord(int groupId) const
{
    return m_names.contains(groupId) || m_parents.contains(groupId);
}

void GroupDomain::RemoveRecord(int groupId)
{
    if (groupId <= 0) {
        return;
    }
    m_names.remove(groupId);
    m_parents.remove(groupId);
    for (auto it = m_parents.begin(); it != m_parents.end();) {
        if (it.value() == groupId) {
            it = m_parents.erase(it);
        } else {
            ++it;
        }
    }
}

GroupState GroupDomain::Snapshot() const
{
    return GroupState{m_names, m_parents};
}

void GroupDomain::ApplyRecords(const GroupState& groups)
{
    Reset();
    for (auto it = groups.names.constBegin(); it != groups.names.constEnd(); ++it) {
        if (it.key() <= 0 || it.value().trimmed().isEmpty()) {
            continue;
        }
        m_names.insert(it.key(), it.value().trimmed());
        ObserveId(it.key());
    }
    for (auto it = groups.parents.constBegin(); it != groups.parents.constEnd(); ++it) {
        if (it.key() <= 0) {
            continue;
        }
        m_parents.insert(it.key(), it.value());
        ObserveId(it.key());
        ObserveId(it.value());
    }
}
