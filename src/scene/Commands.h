#pragma once

#include "items/CanvasTypes.h"
#include "items/ItemSnapshot.h"
#include "items/ItemStack.h"
#include "items/lines/LineAbstractItem.h"
#include "items/properties/ItemProperties.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "scene/GroupState.h"

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPair>
#include <QPixmap>
#include <QPointer>
#include <QUndoCommand>

class CanvasItem;
class CanvasScene;
class PolygonItem;
class StraightLineItem;

class AddItemCommand : public QUndoCommand {
  public:
    AddItemCommand(CanvasScene* scene, CanvasItem* item, const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
};

class RemoveItemsCommand : public QUndoCommand {
  public:
    RemoveItemsCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    // 删除前的组记录快照：撤销时连同空组状态一起恢复（§7.4）。
    GroupState m_groupsBefore;
    GroupState m_groupsAfter;
};

// 连续方向键微调共用这个命令 id：QUndoStack 只对 id 相同且非 -1 的命令尝试
// mergeWith，普通拖拽/缩放的 GeometryCommand 不带 mergeId，绝不参与合并。
enum { GEOMETRY_NUDGE_COMMAND_ID = 1001 };

class GeometryCommand : public QUndoCommand {
  public:
    GeometryCommand(CanvasScene* scene, const ItemPlacementList& before, const ItemPlacementList& after,
                    const QString& text);
    void undo() override;
    void redo() override;
    // 标记为可合并的连续操作（目前只有 nudge）：同一批图元的相邻两次调用
    // 由 mergeWith 叠加成一条撤销记录，方向键按住不放不再刷爆撤销栈。
    void SetMergeId(const QString& id);
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

  private:
    void Apply(const ItemPlacementList& geometry);

    CanvasScene* m_scene = nullptr;
    ItemPlacementList m_before;
    ItemPlacementList m_after;
    QString m_mergeId;
};

class GroupCommand : public QUndoCommand {
  public:
    // 分组有层级后，撤销不仅要还原成员归属，还要还原组与组的父子关系与组名快照。
    GroupCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<int>& before, const QList<int>& after,
                 const QString& text, const GroupState& groupsBefore = GroupState(),
                 const GroupState& groupsAfter = GroupState());
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<int>& groupIds, const GroupState& groups);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<int> m_before;
    QList<int> m_after;
    GroupState m_groupsBefore;
    GroupState m_groupsAfter;
};

// 组重命名：只改组名，成员归属与父子关系不动。
class GroupRenameCommand : public QUndoCommand {
  public:
    GroupRenameCommand(CanvasScene* scene, int groupId, const QString& before, const QString& after,
                       const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QString& name);

    CanvasScene* m_scene = nullptr;
    int m_groupId = 0;
    QString m_before;
    QString m_after;
};

class BackgroundCommand : public QUndoCommand {
  public:
    BackgroundCommand(CanvasScene* scene, const CanvasBackground& before, const CanvasBackground& after,
                      const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    CanvasBackground m_before;
    CanvasBackground m_after;
};

class CanvasSizeCommand : public QUndoCommand {
  public:
    CanvasSizeCommand(CanvasScene* scene, const QRectF& before, const QRectF& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    QRectF m_before;
    QRectF m_after;
};

class ItemStackCommand : public QUndoCommand {
  public:
    ItemStackCommand(CanvasScene* scene, const ItemStackList& before, const ItemStackList& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    ItemStackList m_before;
    ItemStackList m_after;
};

class TextItemCommand : public QUndoCommand {
  public:
    // 文字编辑的专用 Memento 命令：正文 / 字体 / 颜色 / 自然尺寸一次恢复（§3.6）。
    TextItemCommand(CanvasScene* scene, TextItem* item, const TextItemState& before, const TextItemState& after,
                    const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    QPointer<TextItem> m_item;
    TextItemState m_before;
    TextItemState m_after;
};

// 文字归属图形：绑定 / 解绑都走这一条命令。containerId、文字位置与折行尺寸一起改，
// 解绑时保留文字当前所在位置，撤销回到绑定前的完整文字状态。
class TextContainerCommand : public QUndoCommand {
  public:
    TextContainerCommand(CanvasScene* scene, TextItem* text, const QString& beforeUid, const TextItemState& beforeState,
                         const QString& afterUid, const TextItemState& afterState, const QString& label);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QString& uid, const TextItemState& state);

    CanvasScene* m_scene = nullptr;
    QPointer<TextItem> m_text;
    QString m_beforeUid;
    TextItemState m_beforeState;
    QString m_afterUid;
    TextItemState m_afterState;
};

// 需求 35：旋转手柄与快捷键共用这一条命令，支持一次旋转多个选中图元。
class RotationCommand : public QUndoCommand {
  public:
    RotationCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<qreal>& before,
                    const QList<qreal>& after, const QString& text,
                    const ItemPlacementList& geometryBefore = ItemPlacementList(),
                    const ItemPlacementList& geometryAfter = ItemPlacementList());
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<qreal>& angles, const ItemPlacementList& geometry);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<qreal> m_before;
    QList<qreal> m_after;
    ItemPlacementList m_geometryBefore;
    ItemPlacementList m_geometryAfter;
};

// 三型互换：撤销时换回旧对象，重做时换回新对象；两个对象都由命令持有。
// 原图元带容器文字时，文字归属跟着换到新 uid，撤销再指回旧 uid，避免留下孤儿文字。
class ConvertItemCommand : public QUndoCommand {
  public:
    ConvertItemCommand(CanvasScene* scene, CanvasItem* before, CanvasItem* after,
                       const QList<TextItem*>& containerTexts, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Swap(CanvasItem* detach, CanvasItem* attach);

    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_before;
    QPointer<CanvasItem> m_after;
    QList<QPointer<TextItem>> m_containerTexts;
};

// 两端绑定直线的走线形态切换：直连 / 折线，形态标记与重排后的几何同进同退。
class LineRouteCommand : public QUndoCommand {
  public:
    LineRouteCommand(CanvasScene* scene, StraightLineItem* line, int beforeRoute,
                     const LineAbstractItem::GeometryState& beforeGeometry, int afterRoute,
                     const LineAbstractItem::GeometryState& afterGeometry, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(int route, const LineAbstractItem::GeometryState& geometry);

    CanvasScene* m_scene = nullptr;
    QPointer<LineAbstractItem> m_line;
    int m_beforeRoute = 0;
    int m_afterRoute = 0;
    LineAbstractItem::GeometryState m_beforeGeometry;
    LineAbstractItem::GeometryState m_afterGeometry;
};

// 镜像翻转的一次提交：每个图元的原位 / 原旋转角 / 原翻转位与镜像后的对应值。
struct MirrorItemState {
    QPointer<CanvasItem> item;
    QPointF pos;
    qreal rotation = 0.0;
    bool flipX = false;
    bool flipY = false;
};

// 左右 / 上下镜像共用这条命令；翻转位是显示级状态，位置与旋转角才是几何改动。
class MirrorCommand : public QUndoCommand {
  public:
    MirrorCommand(CanvasScene* scene, const QList<MirrorItemState>& before, const QList<MirrorItemState>& after,
                  const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<MirrorItemState>& states);

    CanvasScene* m_scene = nullptr;
    QList<MirrorItemState> m_before;
    QList<MirrorItemState> m_after;
};

// 手动平滑一次可能同时作用于多条选中笔画，整体只占一条撤销记录。
class StrokeSmoothCommand : public QUndoCommand {
  public:
    StrokeSmoothCommand(CanvasScene* scene, const QList<LineAbstractItem*>& items,
                        const QList<LineAbstractItem::GeometryState>& before,
                        const QList<LineAbstractItem::GeometryState>& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<LineAbstractItem::GeometryState>& states);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<LineAbstractItem>> m_items;
    QList<LineAbstractItem::GeometryState> m_before;
    QList<LineAbstractItem::GeometryState> m_after;
};

// 箭头两端绑定的一份快照，端点拖拽 / 删除目标时用它做可撤销的解除与恢复。
struct ArrowBindingState {
    QJsonObject start;
    QJsonObject end;
};

class ArrowBindingCommand : public QUndoCommand {
  public:
    ArrowBindingCommand(CanvasScene* scene, const QList<LineAbstractItem*>& items,
                        const QList<ArrowBindingState>& before, const QList<ArrowBindingState>& after,
                        const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<ArrowBindingState>& states);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<LineAbstractItem>> m_items;
    QList<ArrowBindingState> m_before;
    QList<ArrowBindingState> m_after;
};

// 透明度对图形、笔画、文字、图片等所有图元都生效。
// 属性表命令：一次编辑把同一组属性值写回所有目标图元，只占一条撤销记录。
// 替代原先按图元类型分拆的样式命令，读写都走 Properties()/PropertyValue()/SetPropertyValue()。
class ItemPropertyCommand : public QUndoCommand {
  public:
    ItemPropertyCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                        const QList<QMap<PropertyKey, QVariant>>& before,
                        const QList<QMap<PropertyKey, QVariant>>& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<QMap<PropertyKey, QVariant>>& states);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<QMap<PropertyKey, QVariant>> m_before;
    QList<QMap<PropertyKey, QVariant>> m_after;
};

class LayerStateCommand : public QUndoCommand {
  public:
    LayerStateCommand(CanvasScene* scene, const LayerStackState& before, const LayerStackState& after,
                      const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    LayerStackState m_before;
    LayerStackState m_after;
};
