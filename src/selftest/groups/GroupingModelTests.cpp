#include "items/CanvasItem.h"
#include "items/CanvasTypes.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/shapes/ShapeFactory.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"
#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QJsonObject>
#include <QPointF>
#include <QSizeF>
#include <QUndoStack>

namespace {

CanvasItem* AddTestRect(CanvasScene& scene, const QPointF& pos, const QSizeF& size, int layerId = -1)
{
    auto* item = ShapeFactory::Create(ShapeType::RECTANGLE);
    item->SetContentSize(size);
    item->setPos(pos);
    if (layerId >= 0) {
        item->SetLayerId(layerId);
    }
    scene.AddItemsWithUndo({item}, QStringLiteral("Add test item"));
    return item;
}

} // namespace

void selftest::RunGroupingModelTests(Context& ctx)
{
    // 失败枚举：非编辑模式 / 少于两个 / 跨层；同层多选可分组。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);

        auto* first = AddTestRect(scene, QPointF(80.0, 90.0), QSizeF(90.0, 70.0));
        auto* second = AddTestRect(scene, QPointF(320.0, 260.0), QSizeF(90.0, 70.0));

        GroupFailureReason reason = GroupFailureReason::NONE;
        first->setSelected(true);
        scene.CanGroupSelectedItems(&reason);
        ctx.Check(reason == GroupFailureReason::NOT_ENOUGH_ITEMS, QStringLiteral("少于两个图元不能分组"));

        scene.AddLayerWithUndo(QStringLiteral("Layer 2"));
        const int secondLayer = scene.Layers().last().id;
        second->SetLayerId(secondLayer);
        first->setSelected(true);
        second->setSelected(true);
        reason = GroupFailureReason::NONE;
        ctx.Check(!scene.CanGroupSelectedItems(&reason) && reason == GroupFailureReason::DIFFERENT_LAYERS,
                  QStringLiteral("跨层图元不能分组"));
        ctx.Check(scene.GroupSelectedItems(&reason) == 0, QStringLiteral("跨层分组失败不落组"));

        second->SetLayerId(first->LayerId());
        reason = GroupFailureReason::NONE;
        const int grouped = scene.GroupSelectedItems(&reason);
        const int groupId = first->GroupId();
        ctx.Check(grouped == 2 && reason == GroupFailureReason::NONE && groupId > 0 && second->GroupId() == groupId,
                  QStringLiteral("同层多选图元可分组"));

        // 查看模式下不允许分组。
        CanvasScene viewModeScene;
        QUndoStack viewModeUndo;
        viewModeScene.SetUndoStack(&viewModeUndo);
        viewModeScene.SetMode(ToolMode::EDIT);
        auto* viewA = AddTestRect(viewModeScene, QPointF(20.0, 20.0), QSizeF(60.0, 40.0));
        auto* viewB = AddTestRect(viewModeScene, QPointF(120.0, 20.0), QSizeF(60.0, 40.0));
        viewModeScene.SetMode(ToolMode::VIEW);
        viewA->setSelected(true);
        viewB->setSelected(true);
        GroupFailureReason viewReason = GroupFailureReason::NONE;
        ctx.Check(!viewModeScene.CanGroupSelectedItems(&viewReason) && viewReason == GroupFailureReason::NOT_EDIT_MODE,
                  QStringLiteral("非编辑模式不能分组"));
    }

    // 组名唯一、重命名校验与撤销；空组清理。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);

        auto* first = AddTestRect(scene, QPointF(80.0, 90.0), QSizeF(90.0, 70.0));
        auto* second = AddTestRect(scene, QPointF(320.0, 260.0), QSizeF(90.0, 70.0));
        first->setSelected(true);
        second->setSelected(true);
        scene.GroupSelectedItems();
        const int groupId = first->GroupId();

        ctx.Check(scene.GroupName(groupId) == QStringLiteral("组 1"), QStringLiteral("新组自动命名为组 1"));
        ctx.Check(!scene.IsGroupNameAvailable(QStringLiteral("组 1"), 0), QStringLiteral("已占用组名不可再用"));
        ctx.Check(scene.IsGroupNameAvailable(QStringLiteral("Hero"), groupId), QStringLiteral("未占用组名可用"));
        ctx.Check(scene.UniqueGroupName(QStringLiteral("组")) == QStringLiteral("组"),
                  QStringLiteral("未占用的 base 直接可用"));

        ctx.Check(scene.RenameGroupWithUndo(groupId, QStringLiteral("Hero")), QStringLiteral("合法组名可重命名"));
        ctx.Check(scene.GroupName(groupId) == QStringLiteral("Hero"), QStringLiteral("重命名后组名生效"));
        ctx.Check(!scene.RenameGroupWithUndo(groupId, QString()), QStringLiteral("空组名不提交"));
        ctx.Check(!scene.RenameGroupWithUndo(groupId, QStringLiteral("Hero")), QStringLiteral("同名不提交重命名"));

        undo.undo();
        ctx.Check(scene.GroupName(groupId) == QStringLiteral("组 1"), QStringLiteral("撤销重命名恢复原组名"));
        undo.redo();
        ctx.Check(scene.GroupName(groupId) == QStringLiteral("Hero"), QStringLiteral("重做重命名恢复新组名"));

        scene.clearSelection();
        first->setSelected(true);
        second->setSelected(true);
        scene.DeleteSelected();
        ctx.Check(scene.GroupIds().isEmpty() && scene.GroupNames().isEmpty(),
                  QStringLiteral("删除全部组员后空组被清理"));
    }

    // 组派生查询：成员数、组框、所属图层、整组叶子。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.SetMode(ToolMode::EDIT);

        auto* first = AddTestRect(scene, QPointF(80.0, 90.0), QSizeF(90.0, 70.0));
        auto* second = AddTestRect(scene, QPointF(320.0, 260.0), QSizeF(90.0, 70.0));
        auto* third = AddTestRect(scene, QPointF(200.0, 180.0), QSizeF(80.0, 60.0));
        QList<CanvasItem*> items{first, second, third};

        GroupFailureReason reason = GroupFailureReason::NONE;
        const int groupId = scene.AssignNewGroup(items, &reason);
        ctx.Check(groupId > 0 && reason == GroupFailureReason::NONE, QStringLiteral("插入即成组"));
        ctx.Check(scene.MemberCount(groupId) == 3, QStringLiteral("组员计数为 3"));
        ctx.Check(!scene.GroupBounds(groupId).isNull(), QStringLiteral("组框为成员外框并集"));
        ctx.Check(scene.GroupLayerId(groupId) == first->LayerId(), QStringLiteral("组所属图层取成员图层"));
        ctx.Check(scene.AllLeafItems(groupId).size() == 3, QStringLiteral("整组叶子图元齐全"));

        // 序列化：组名落盘；不写任何组级旋转 / 变换字段。
        const QJsonObject root = scene.ToJson();
        ctx.Check(root.contains(QStringLiteral("groupNames")), QStringLiteral("组名随画布保存"));
        ctx.Check(!root.contains(QStringLiteral("groupRotation")) && !root.contains(QStringLiteral("groupTransform")),
                  QStringLiteral("组旋转不持久化"));

        CanvasScene reloaded;
        ctx.Check(reloaded.FromJson(root), QStringLiteral("分组画布可重新载入"));
        ctx.Check(reloaded.GroupIds().size() == 1, QStringLiteral("载入后组记录完整"));
        ctx.Check(reloaded.GroupName(reloaded.GroupIds().first()) == scene.GroupName(groupId),
                  QStringLiteral("载入后组名保留"));

        // 完整整组选中才命中组选框；只选部分成员不命中。
        int selectedGroup = 0;
        scene.clearSelection();
        first->setSelected(true);
        ctx.Check(!scene.SelectionIsCompleteGroup(&selectedGroup), QStringLiteral("部分成员选中不命中组选框"));
        first->setSelected(true);
        second->setSelected(true);
        third->setSelected(true);
        ctx.Check(scene.SelectionIsCompleteGroup(&selectedGroup) && selectedGroup == groupId,
                  QStringLiteral("完整整组选中命中组选框"));
    }

    // 旧画布跨层组：载入时清理组关系，图元保留。
    {
        CanvasScene legacy;
        QUndoStack legacyUndo;
        legacy.SetUndoStack(&legacyUndo);
        legacy.SetMode(ToolMode::EDIT);
        auto* left = AddTestRect(legacy, QPointF(20.0, 20.0), QSizeF(60.0, 40.0));
        auto* right = AddTestRect(legacy, QPointF(160.0, 20.0), QSizeF(60.0, 40.0));
        legacy.AddLayerWithUndo(QStringLiteral("Legacy layer"));
        const int legacyLayer2 = legacy.Layers().last().id;
        right->SetLayerId(legacyLayer2);
        left->SetGroupId(1);
        right->SetGroupId(1);

        CanvasScene loaded;
        loaded.FromJson(legacy.ToJson());
        bool stillGrouped = false;
        for (CanvasItem* item : loaded.CanvasItems()) {
            if (item->GroupId() > 0) {
                stillGrouped = true;
            }
        }
        ctx.Check(!stillGrouped, QStringLiteral("跨层旧组载入时清理组关系"));
    }

    // AssignNewGroup 校验：单个失败、跨层失败、同层成功。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.SetMode(ToolMode::EDIT);
        auto* first = AddTestRect(scene, QPointF(20.0, 20.0), QSizeF(60.0, 40.0));
        auto* second = AddTestRect(scene, QPointF(160.0, 20.0), QSizeF(60.0, 40.0));

        GroupFailureReason reason = GroupFailureReason::NONE;
        ctx.Check(scene.AssignNewGroup({first}, &reason) == 0 && reason == GroupFailureReason::NOT_ENOUGH_ITEMS,
                  QStringLiteral("单个图元不能成组"));

        scene.AddLayerWithUndo(QStringLiteral("Assign layer"));
        const int otherLayer = scene.Layers().last().id;
        second->SetLayerId(otherLayer);
        ctx.Check(scene.AssignNewGroup({first, second}, &reason) == 0 && reason == GroupFailureReason::DIFFERENT_LAYERS,
                  QStringLiteral("跨层插入即成组失败"));

        second->SetLayerId(first->LayerId());
        ctx.Check(scene.AssignNewGroup({first, second}, &reason) > 0 && reason == GroupFailureReason::NONE,
                  QStringLiteral("同层插入即成组成功"));
    }

    // 图层拖拽排序：一条撤销记录可还原；背景层与同下标不动作。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.AddLayerWithUndo(QStringLiteral("Layer A"));
        scene.AddLayerWithUndo(QStringLiteral("Layer B"));
        scene.AddLayerWithUndo(QStringLiteral("Layer C"));

        const QList<CanvasLayer> layers = scene.Layers();
        const int idA = layers.at(1).id;
        const int idB = layers.at(2).id;
        const int idC = layers.at(3).id;

        ctx.Check(scene.MoveLayerToIndexWithUndo(idC, 1), QStringLiteral("图层可拖拽到目标下标"));
        const QList<CanvasLayer> reordered = scene.Layers();
        ctx.Check(reordered.at(1).id == idC && reordered.at(2).id == idA && reordered.at(3).id == idB,
                  QStringLiteral("拖拽后图层顺序正确"));

        undo.undo();
        const QList<CanvasLayer> restored = scene.Layers();
        ctx.Check(restored.at(1).id == idA && restored.at(2).id == idB && restored.at(3).id == idC,
                  QStringLiteral("撤销拖拽恢复图层顺序"));

        ctx.Check(!scene.MoveLayerToIndexWithUndo(CanvasScene::BackgroundLayerId, 1),
                  QStringLiteral("背景层不能参与排序"));
        ctx.Check(!scene.MoveLayerToIndexWithUndo(idA, scene.LayerIndex(idA)), QStringLiteral("同下标不动作"));
    }

    // §7.5 / D6：组内成员不允许单独换层，整组一起换层才放行，并被拒绝时给出提示。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.ApplyCanvasSize(QSizeF(800.0, 600.0));
        scene.SetMode(ToolMode::EDIT);
        auto* first = AddTestRect(scene, QPointF(80.0, 90.0), QSizeF(90.0, 70.0));
        auto* second = AddTestRect(scene, QPointF(320.0, 260.0), QSizeF(90.0, 70.0));
        const int baseLayer = first->LayerId();
        scene.AddLayerWithUndo(QStringLiteral("Target layer"));
        const int targetLayer = scene.Layers().last().id;

        ctx.Check(scene.CanMoveItemToLayer(first, targetLayer), QStringLiteral("未分组图元可以单独换层"));

        first->setSelected(true);
        second->setSelected(true);
        scene.GroupSelectedItems();
        const int groupId = first->GroupId();
        ctx.Check(groupId > 0 && second->GroupId() == groupId, QStringLiteral("两图元已成组"));

        int hints = 0;
        QObject::connect(&scene, &CanvasScene::statusMessage, &scene, [&hints](const QString&) { ++hints; });

        first->setSelected(true);
        second->setSelected(false);
        ctx.Check(!scene.CanMoveItemToLayer(first, targetLayer), QStringLiteral("组内成员不能单独换层"));
        ctx.Check(scene.CanMoveItemToLayer(first, baseLayer), QStringLiteral("组内成员留在本层不算换层"));
        ctx.Check(!scene.MoveItemToAdjacentLayer(first, 1), QStringLiteral("相邻层移动同样拒绝组内成员单独换层"));
        ctx.Check(hints == 1, QStringLiteral("拒绝换层时给出一次状态栏提示"), QStringLiteral("hints=%1").arg(hints));

        first->setSelected(true);
        second->setSelected(true);
        ctx.Check(scene.CanMoveItemToLayer(first, targetLayer), QStringLiteral("整组一起换层放行"));
    }

    // 删除嵌套组的子组后撤销：组名、成员归属与父组关系必须整体恢复。
    {
        CanvasScene scene;
        QUndoStack undo;
        scene.SetUndoStack(&undo);
        scene.ApplyCanvasSize(QSizeF(900.0, 700.0));
        scene.SetMode(ToolMode::EDIT);

        auto* first = AddTestRect(scene, QPointF(60.0, 80.0), QSizeF(80.0, 60.0));
        auto* second = AddTestRect(scene, QPointF(220.0, 80.0), QSizeF(80.0, 60.0));
        auto* third = AddTestRect(scene, QPointF(380.0, 80.0), QSizeF(80.0, 60.0));
        auto* fourth = AddTestRect(scene, QPointF(540.0, 80.0), QSizeF(80.0, 60.0));

        first->setSelected(true);
        second->setSelected(true);
        scene.GroupSelectedItems();
        const int childGroup = first->GroupId();
        const QString childGroupName = scene.GroupName(childGroup);
        scene.clearSelection();
        third->setSelected(true);
        fourth->setSelected(true);
        scene.GroupSelectedItems();
        const int otherGroup = third->GroupId();

        scene.clearSelection();
        first->setSelected(true);
        third->setSelected(true);
        scene.GroupSelectedItems();
        const int outerGroup = scene.ParentGroup(childGroup);
        const QString outerGroupName = scene.GroupName(outerGroup);
        ctx.Check(outerGroup > 0 && scene.ParentGroup(otherGroup) == outerGroup,
                  QStringLiteral("嵌套分组记录父子关系"));

        undo.push(new RemoveItemsCommand(&scene, {first, second}, QStringLiteral("Remove nested group")));
        ctx.Check(scene.GroupName(childGroup).isEmpty() && scene.GroupParents().value(childGroup) == 0,
                  QStringLiteral("删除子组全部成员后清理空组与父关系"));

        undo.undo();
        ctx.Check(scene.GroupName(childGroup) == childGroupName && scene.GroupName(outerGroup) == outerGroupName,
                  QStringLiteral("撤销删除恢复子组名与父组名"));
        ctx.Check(first->GroupId() == childGroup && second->GroupId() == childGroup,
                  QStringLiteral("撤销删除恢复成员归属"));
        ctx.Check(scene.ParentGroup(childGroup) == outerGroup, QStringLiteral("撤销删除恢复子组父关系"));
    }
}
