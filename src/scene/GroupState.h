#pragma once

#include <QHash>
#include <QString>

// 分组命令的 before/after 快照：组名与父子关系总是一起恢复，避免两份字段各自漂移。
struct GroupState {
    QHash<int, QString> names;
    QHash<int, int> parents;

    bool operator==(const GroupState& other) const { return names == other.names && parents == other.parents; }
    bool operator!=(const GroupState& other) const { return !(*this == other); }
};
