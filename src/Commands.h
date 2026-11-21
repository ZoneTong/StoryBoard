#pragma once

#include "CanvasTypes.h"
#include "ItemGeometry.h"
#include "ItemProperties.h"
#include "LineItemAbstract.h"
#include "ShapeItem.h"
#include "TextItem.h"

#include <QColor>
#include <QList>
#include <QMap>
#include <QPair>
#include <QPixmap>
#include <QPointer>
#include <QUndoCommand>

class CanvasItem;
class CanvasScene;
class PolygonItem;

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
};

// 连续方向键微调共用这个命令 id：QUndoStack 只对 id 相同且非 -1 的命令尝试
// mergeWith，普通拖拽/缩放的 GeometryCommand 不带 mergeId，绝不参与合并。
enum { GEOMETRY_NUDGE_COMMAND_ID = 1001 };

class GeometryCommand : public QUndoCommand {
  public:
    GeometryCommand(CanvasScene* scene, const ItemGeometryList& before, const ItemGeometryList& after,
                    const QString& text);
    void undo() override;
    void redo() override;
    // 标记为可合并的连续操作（目前只有 nudge）：同一批图元的相邻两次调用
    // 由 mergeWith 叠加成一条撤销记录，方向键按住不放不再刷爆撤销栈。
    void SetMergeId(const QString& id);
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

  private:
    void Apply(const ItemGeometryList& geometry);

    CanvasScene* m_scene = nullptr;
    ItemGeometryList m_before;
    ItemGeometryList m_after;
    QString m_mergeId;
};

class GroupCommand : public QUndoCommand {
  public:
    // 分组有层级后，撤销不仅要还原成员归属，还要还原组与组的父子关系。
    GroupCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<int>& before, const QList<int>& after,
                 const QString& text, const QHash<int, int>& parentsBefore = QHash<int, int>(),
                 const QHash<int, int>& parentsAfter = QHash<int, int>());
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<int>& groupIds, const QHash<int, int>& parents);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<int> m_before;
    QList<int> m_after;
    QHash<int, int> m_parentsBefore;
    QHash<int, int> m_parentsAfter;
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
    TextItemCommand(CanvasScene* scene, CanvasItem* item, const TextItemState& before, const TextItemState& after,
                    const QString& text);
    void undo() override;
    void redo() override;

  private:
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    TextItemState m_before;
    TextItemState m_after;
};

// 需求 35：旋转手柄与快捷键共用这一条命令，支持一次旋转多个选中图元。
class RotationCommand : public QUndoCommand {
  public:
    RotationCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<qreal>& before,
                    const QList<qreal>& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<qreal>& angles);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<qreal> m_before;
    QList<qreal> m_after;
};

// 手动平滑一次可能同时作用于多条选中笔画，整体只占一条撤销记录。
class StrokeSmoothCommand : public QUndoCommand {
  public:
    StrokeSmoothCommand(CanvasScene* scene, const QList<LineItemAbstract*>& items,
                        const QList<LineItemAbstract::GeometryState>& before,
                        const QList<LineItemAbstract::GeometryState>& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<LineItemAbstract::GeometryState>& states);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<LineItemAbstract>> m_items;
    QList<LineItemAbstract::GeometryState> m_before;
    QList<LineItemAbstract::GeometryState> m_after;
};

// 属性弹窗一次修改可能同时涉及层/高度、几何、填充、描边、文字或图片名，
// 用同一个快照命令承载，保证一次编辑只占一条撤销记录。
struct ItemPropertiesState {
    ItemStackList stack;
    QPointF pos;
    QSizeF size;
    qreal rotation = 0.0;
    // 透明度是所有图元共有的属性，属性窗统一按这个字段提交。
    qreal opacity = 1.0;
    QString itemName;
    // 多边形也是 ShapeItem，填充、描线与图片填充都走这一份快照。
    bool hasShapeState = false;
    ShapeItemState shapeState;
    bool hasTextState = false;
    TextItemState textState;
    bool hasStrokeStyle = false;
    QColor strokeColor;
    qreal strokeWidth = 4.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    // 需求 27：曲线两端箭头与直线同一套属性窗提交流程（箭头对全部曲线子型开放）。
    ArrowStyle curveStartArrow = ArrowStyle::NONE;
    ArrowStyle curveEndArrow = ArrowStyle::NONE;

    bool operator==(const ItemPropertiesState& other) const;
    bool operator!=(const ItemPropertiesState& other) const { return !(*this == other); }
};

class ItemPropertiesCommand : public QUndoCommand {
  public:
    ItemPropertiesCommand(CanvasScene* scene, CanvasItem* item, const ItemPropertiesState& before,
                          const ItemPropertiesState& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const ItemPropertiesState& state);

    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    ItemPropertiesState m_before;
    ItemPropertiesState m_after;
};

// 透明度对图形、笔画、文字、图片等所有图元都生效。
// 属性表命令：一次编辑把同一组属性值写回所有目标图元，只占一条撤销记录。
// 替代原先按图元类型分拆的样式命令，读写都走 Properties()/PropertyValue()/SetPropertyValue()。
class ItemPropertyCommand : public QUndoCommand {
  public:
    ItemPropertyCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                        const QList<QMap<ItemProperty, QVariant>>& before,
                        const QList<QMap<ItemProperty, QVariant>>& after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    void Apply(const QList<QMap<ItemProperty, QVariant>>& states);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<QMap<ItemProperty, QVariant>> m_before;
    QList<QMap<ItemProperty, QVariant>> m_after;
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
