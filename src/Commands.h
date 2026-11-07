#pragma once

#include <QPointer>
#include <QUndoCommand>
#include <QList>

#include "BoardTypes.h"
#include "ItemGeometry.h"
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
    BoardSizeCommand(BoardScene* scene, const QSizeF& before, const QSizeF& after, const QString& text);
    void undo() override;
    void redo() override;

private:
    BoardScene* m_scene = nullptr;
    QSizeF m_before;
    QSizeF m_after;
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
