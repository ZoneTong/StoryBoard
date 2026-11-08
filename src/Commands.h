#pragma once

#include <QColor>
#include <QPixmap>
#include <QPointer>
#include <QUndoCommand>
#include <QList>

#include "CanvasTypes.h"
#include "ItemGeometry.h"
#include "ShapeItem.h"
#include "TextItem.h"

class CanvasItem;
class CanvasScene;

class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(CanvasScene* scene, CanvasItem* item, const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
};

class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
};

class GeometryCommand : public QUndoCommand
{
public:
    GeometryCommand(CanvasScene* scene, const ItemGeometryList& before,
                    const ItemGeometryList& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const ItemGeometryList& geometry);

    CanvasScene* m_scene = nullptr;
    ItemGeometryList m_before;
    ItemGeometryList m_after;
};

class BackgroundCommand : public QUndoCommand
{
public:
    BackgroundCommand(CanvasScene* scene, const BackgroundState& before,
                      const BackgroundState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    BackgroundState m_before;
    BackgroundState m_after;
};

class CanvasSizeCommand : public QUndoCommand
{
public:
    CanvasSizeCommand(CanvasScene* scene, const QRectF& before, const QRectF& after,
                     const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    QRectF m_before;
    QRectF m_after;
};

class ItemStackCommand : public QUndoCommand
{
public:
    ItemStackCommand(CanvasScene* scene, const ItemStackList& before,
                     const ItemStackList& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    ItemStackList m_before;
    ItemStackList m_after;
};

class TextItemCommand : public QUndoCommand
{
public:
    TextItemCommand(CanvasScene* scene, TextItem* item, const TextItemState& before,
                    const TextItemState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    QPointer<TextItem> m_item;
    TextItemState m_before;
    TextItemState m_after;
};

class ShapeStyleCommand : public QUndoCommand
{
public:
    ShapeStyleCommand(CanvasScene* scene, const QList<ShapeItem*>& items,
                      const QList<ShapeItemState>& before,
                      const QList<ShapeItemState>& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const QList<ShapeItemState>& states);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<ShapeItem>> m_items;
    QList<ShapeItemState> m_before;
    QList<ShapeItemState> m_after;
};

// 需求 35：旋转手柄与快捷键共用这一条命令，支持一次旋转多个选中图元。
class RotationCommand : public QUndoCommand
{
public:
    RotationCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                    const QList<qreal>& before, const QList<qreal>& after,
                    const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const QList<qreal>& angles);

    CanvasScene* m_scene = nullptr;
    QList<QPointer<CanvasItem>> m_items;
    QList<qreal> m_before;
    QList<qreal> m_after;
};

// 属性弹窗一次修改可能同时涉及层/高度、几何、填充、描边、文字或图片名，
// 用同一个快照命令承载，保证一次编辑只占一条撤销记录。
struct ItemPropertiesState {
    ItemStackList stack;
    QPointF pos;
    QSizeF size;
    qreal rotation = 0.0;
    QString itemName;
    bool hasShapeState = false;
    ShapeItemState shapeState;
    bool hasTextState = false;
    TextItemState textState;
    bool hasStrokeStyle = false;
    QColor strokeColor;
    qreal strokeWidth = 4.0;
    // 需求 39：图片本体也是可编辑属性，替换图片与改名一起进入同一条撤销记录。
    bool hasImageState = false;
    QPixmap image;
    bool hasImageName = false;
    QString imageName;

    bool operator==(const ItemPropertiesState& other) const;
    bool operator!=(const ItemPropertiesState& other) const { return !(*this == other); }
};

class ItemPropertiesCommand : public QUndoCommand
{
public:
    ItemPropertiesCommand(CanvasScene* scene, CanvasItem* item,
                          const ItemPropertiesState& before,
                          const ItemPropertiesState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const ItemPropertiesState& state);

    CanvasScene* m_scene = nullptr;
    QPointer<CanvasItem> m_item;
    ItemPropertiesState m_before;
    ItemPropertiesState m_after;
};

class LayerStateCommand : public QUndoCommand
{
public:
    LayerStateCommand(CanvasScene* scene, const LayerStackState& before,
                      const LayerStackState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    CanvasScene* m_scene = nullptr;
    LayerStackState m_before;
    LayerStackState m_after;
};
