#include "ui/panels/layers/LayerTreeModel.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"

#include <QSet>
#include <algorithm>

namespace {

// 同一父节点内的图元行：层内高度降序；高度相同时按名称稳定排序，保证脏文档也能确定。
bool HeightFirst(const CanvasItem* left, const CanvasItem* right)
{
    if (left->ZOrder() != right->ZOrder()) {
        return left->ZOrder() > right->ZOrder();
    }
    const QString leftName = left->Name();
    const QString rightName = right->Name();
    const int compared = QString::compare(leftName, rightName, Qt::CaseInsensitive);
    if (compared != 0) {
        return compared < 0;
    }
    return leftName < rightName;
}

// 组子树的事实：子树叶子图元、是否单层、层号。跨层组的层号为 0，不出组行。
struct GroupFacts {
    QList<CanvasItem*> items;
    int layerId = 0;
    bool singleLayer = true;
};

class Builder {
  public:
    explicit Builder(CanvasScene* scene) : m_scene(scene) {}

    QHash<int, QList<LayerTreeRow>> Build(bool grouped);

  private:
    GroupFacts Resolve(int groupId);
    void AppendGroup(int groupId, int depth, int layerId, QList<LayerTreeRow>& rows);
    QList<int> SortedGroups(const QList<int>& ids) const;

    CanvasScene* m_scene = nullptr;
    QSet<int> m_knownGroups;
    QHash<int, QList<CanvasItem*>> m_directMembers; // 组 id → 直属图元
    QHash<int, QList<CanvasItem*>> m_looseItems;    // 图层 id → 未分组图元
    QHash<int, QList<int>> m_children;              // 父组 id → 子组 id
    QList<int> m_rootGroups;
    QHash<int, GroupFacts> m_facts;
    QSet<int> m_visiting;
    QSet<int> m_visited;
};

GroupFacts Builder::Resolve(int groupId)
{
    const auto found = m_facts.constFind(groupId);
    if (found != m_facts.constEnd()) {
        return found.value();
    }
    if (m_visiting.contains(groupId)) {
        // 父子关系成环的坏文档：按空组处理，避免无限递归。
        GroupFacts cycle;
        cycle.singleLayer = false;
        m_facts.insert(groupId, cycle);
        return cycle;
    }

    m_visiting.insert(groupId);
    GroupFacts facts;
    facts.items = m_directMembers.value(groupId);
    int layer = -1;
    for (const CanvasItem* item : std::as_const(facts.items)) {
        if (layer < 0) {
            layer = item->LayerId();
        } else if (layer != item->LayerId()) {
            facts.singleLayer = false;
        }
    }
    for (int child : m_children.value(groupId)) {
        const GroupFacts childFacts = Resolve(child);
        facts.items.append(childFacts.items);
        if (!childFacts.singleLayer) {
            facts.singleLayer = false;
        } else if (layer < 0) {
            layer = childFacts.layerId;
        } else if (layer != childFacts.layerId) {
            facts.singleLayer = false;
        }
    }
    m_visiting.remove(groupId);
    facts.layerId = facts.singleLayer ? qMax(0, layer) : 0;
    m_facts.insert(groupId, facts);
    return facts;
}

QList<int> Builder::SortedGroups(const QList<int>& ids) const
{
    QList<int> sorted = ids;
    std::stable_sort(sorted.begin(), sorted.end(), [this](int left, int right) {
        const int compared = m_scene->GroupName(left).compare(m_scene->GroupName(right), Qt::CaseInsensitive);
        if (compared != 0) {
            return compared < 0;
        }
        return left < right;
    });
    return sorted;
}

void Builder::AppendGroup(int groupId, int depth, int layerId, QList<LayerTreeRow>& rows)
{
    // 环或重复引用：同一位置只展开一次。
    if (m_visited.contains(groupId)) {
        return;
    }
    m_visited.insert(groupId);

    const GroupFacts facts = Resolve(groupId);
    const bool renderable = facts.singleLayer && facts.layerId == layerId && !facts.items.isEmpty();
    if (renderable) {
        rows.append({LayerTreeRow::GROUP, nullptr, groupId, depth, static_cast<int>(facts.items.size())});
    }
    // 跨层组 / 空组不占行位，子组与直属图元上提到父级同一层。
    const int childDepth = renderable ? depth + 1 : depth;
    for (int child : SortedGroups(m_children.value(groupId))) {
        AppendGroup(child, childDepth, layerId, rows);
    }

    QList<CanvasItem*> direct;
    const QList<CanvasItem*> members = m_directMembers.value(groupId);
    for (CanvasItem* item : members) {
        if (item->LayerId() == layerId) {
            direct.append(item);
        }
    }
    std::stable_sort(direct.begin(), direct.end(), HeightFirst);
    for (CanvasItem* item : std::as_const(direct)) {
        rows.append({LayerTreeRow::ITEM, item, 0, childDepth, 0});
    }
}

QHash<int, QList<LayerTreeRow>> Builder::Build(bool grouped)
{
    QHash<int, QList<LayerTreeRow>> result;
    if (!m_scene) {
        return result;
    }

    const QList<CanvasItem*> items = m_scene->CanvasItems();
    const QList<int> groupIds = m_scene->GroupIds();
    const QHash<int, int> parents = m_scene->GroupParents();
    for (int groupId : groupIds) {
        m_knownGroups.insert(groupId);
    }
    for (CanvasItem* item : std::as_const(items)) {
        const int groupId = item->GroupId();
        if (groupId > 0 && m_knownGroups.contains(groupId)) {
            m_directMembers[groupId].append(item);
        } else {
            m_looseItems[item->LayerId()].append(item);
        }
    }
    for (int groupId : groupIds) {
        const int parent = parents.value(groupId, 0);
        if (parent > 0 && parent != groupId && m_knownGroups.contains(parent)) {
            m_children[parent].append(groupId);
        } else {
            m_rootGroups.append(groupId);
        }
    }

    for (const CanvasLayer& layer : m_scene->Layers()) {
        m_visited.clear();
        QList<LayerTreeRow>& rows = result[layer.id];
        if (grouped) {
            for (int root : SortedGroups(m_rootGroups)) {
                AppendGroup(root, 0, layer.id, rows);
            }
            // 父链成环、无法从根到达的组：仍按根展开一次，保证成员不漏。
            for (int groupId : groupIds) {
                if (!m_visited.contains(groupId)) {
                    AppendGroup(groupId, 0, layer.id, rows);
                }
            }
            QList<CanvasItem*> loose = m_looseItems.value(layer.id);
            std::stable_sort(loose.begin(), loose.end(), HeightFirst);
            for (CanvasItem* item : std::as_const(loose)) {
                rows.append({LayerTreeRow::ITEM, item, 0, 0, 0});
            }
        } else {
            QList<CanvasItem*> layerItems;
            for (CanvasItem* item : std::as_const(items)) {
                if (item->LayerId() == layer.id) {
                    layerItems.append(item);
                }
            }
            std::stable_sort(layerItems.begin(), layerItems.end(), HeightFirst);
            for (CanvasItem* item : std::as_const(layerItems)) {
                rows.append({LayerTreeRow::ITEM, item, 0, 0, 0});
            }
        }
    }
    return result;
}

} // namespace

QString LayerTreeModel::IndentText(int depth)
{
    return QString(qMax(0, depth) + 3, QChar(0x2003));
}

QHash<int, QList<LayerTreeRow>> LayerTreeModel::BuildAll(CanvasScene* scene, bool grouped)
{
    return Builder(scene).Build(grouped);
}
