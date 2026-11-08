#pragma once

#include <QPointer>
#include <QUndoCommand>
#include <QList>

#include "BoardTypes.h"
#include "ItemGeometry.h"
#include "ShapeItem.h"
#include "TextItem.h"

class BoardItem;
class BoardScene;

class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(BoardScene* scene, BoardItem* item, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    QPointer<BoardItem> m_item;
};

class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(BoardScene* scene, const QList<BoardItem*>& items, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    QList<QPointer<BoardItem>> m_items;
};

class GeometryCommand : public QUndoCommand
{
public:
    GeometryCommand(BoardScene* scene, const ItemGeometryList& before,
                    const ItemGeometryList& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const ItemGeometryList& geometry);

    BoardScene* m_scene = nullptr;
    ItemGeometryList m_before;
    ItemGeometryList m_after;
};

class BackgroundCommand : public QUndoCommand
{
public:
    BackgroundCommand(BoardScene* scene, const BackgroundState& before,
                      const BackgroundState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    BackgroundState m_before;
    BackgroundState m_after;
};

class BoardSizeCommand : public QUndoCommand
{
public:
    BoardSizeCommand(BoardScene* scene, const QRectF& before, const QRectF& after,
                     const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    QRectF m_before;
    QRectF m_after;
};

class ItemStackCommand : public QUndoCommand
{
public:
    ItemStackCommand(BoardScene* scene, const ItemStackList& before,
                     const ItemStackList& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    ItemStackList m_before;
    ItemStackList m_after;
};

class TextItemCommand : public QUndoCommand
{
public:
    TextItemCommand(BoardScene* scene, TextItem* item, const TextItemState& before,
                    const TextItemState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    QPointer<TextItem> m_item;
    TextItemState m_before;
    TextItemState m_after;
};

class ShapeStyleCommand : public QUndoCommand
{
public:
    ShapeStyleCommand(BoardScene* scene, const QList<ShapeItem*>& items,
                      const QList<ShapeItemState>& before,
                      const QList<ShapeItemState>& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const QList<ShapeItemState>& states);

    BoardScene* m_scene = nullptr;
    QList<QPointer<ShapeItem>> m_items;
    QList<ShapeItemState> m_before;
    QList<ShapeItemState> m_after;
};

// 属性弹窗一次修改可能同时涉及层/高度、几何、填充、描边、文字或图片名，
// 用同一个快照命令承载，保证一次编辑只占一条撤销记录。
struct ItemPropertiesState {
    ItemStackList stack;
    QPointF pos;
    QSizeF size;
    QString itemName;
    bool hasShapeState = false;
    ShapeItemState shapeState;
    bool hasTextState = false;
    TextItemState textState;
    bool hasStrokeStyle = false;
    QColor strokeColor;
    qreal strokeWidth = 4.0;
    bool hasImageName = false;
    QString imageName;

    bool operator==(const ItemPropertiesState& other) const;
    bool operator!=(const ItemPropertiesState& other) const { return !(*this == other); }
};

class ItemPropertiesCommand : public QUndoCommand
{
public:
    ItemPropertiesCommand(BoardScene* scene, BoardItem* item,
                          const ItemPropertiesState& before,
                          const ItemPropertiesState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    void apply(const ItemPropertiesState& state);

    BoardScene* m_scene = nullptr;
    QPointer<BoardItem> m_item;
    ItemPropertiesState m_before;
    ItemPropertiesState m_after;
};

class LayerStateCommand : public QUndoCommand
{
public:
    LayerStateCommand(BoardScene* scene, const LayerStackState& before,
                      const LayerStackState& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    LayerStackState m_before;
    LayerStackState m_after;
};
