#include "Commands.h"

#include "BoardItem.h"
#include "BoardScene.h"

#include <QGraphicsScene>

#include <utility>

AddItemCommand::AddItemCommand(BoardScene* scene, BoardItem* item, const QString& text)
    : m_scene(scene)
    , m_item(item)
{
    setText(text);
}

void AddItemCommand::undo()
{
    if (m_scene && m_item)
        m_scene->detachItem(m_item);
}

void AddItemCommand::redo()
{
    if (m_scene && m_item)
        m_scene->attachItem(m_item);
}

RemoveItemsCommand::RemoveItemsCommand(BoardScene* scene, const QList<BoardItem*>& items,
                                       const QString& text)
    : m_scene(scene)
{
    setText(text);
    for (BoardItem* item : items)
        m_items.append(QPointer<BoardItem>(item));
}

void RemoveItemsCommand::undo()
{
    if (!m_scene)
        return;

    for (const QPointer<BoardItem>& item : std::as_const(m_items)) {
        if (!item)
            continue;
        m_scene->attachItem(item);
        if (m_scene->isItemEditable(item))
            item->setSelected(true);
    }
}

void RemoveItemsCommand::redo()
{
    if (!m_scene)
        return;

    for (const QPointer<BoardItem>& item : std::as_const(m_items)) {
        if (item)
            m_scene->detachItem(item);
    }
}

GeometryCommand::GeometryCommand(BoardScene* scene, const ItemGeometryList& before,
                                 const ItemGeometryList& after, const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void GeometryCommand::undo()
{
    apply(m_before);
}

void GeometryCommand::redo()
{
    apply(m_after);
}

void GeometryCommand::apply(const ItemGeometryList& geometry)
{
    for (const ItemGeometry& entry : geometry) {
        if (!entry.item)
            continue;
        entry.item->setPos(entry.pos);
        entry.item->setContentSize(entry.size);
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}

BackgroundCommand::BackgroundCommand(BoardScene* scene, const BackgroundState& before,
                                     const BackgroundState& after, const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void BackgroundCommand::undo()
{
    if (m_scene)
        m_scene->applyBackground(m_before);
}

void BackgroundCommand::redo()
{
    if (m_scene)
        m_scene->applyBackground(m_after);
}

BoardSizeCommand::BoardSizeCommand(BoardScene* scene, const QSizeF& before, const QSizeF& after,
                                   const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void BoardSizeCommand::undo()
{
    if (m_scene)
        m_scene->applyBoardSize(m_before);
}

void BoardSizeCommand::redo()
{
    if (m_scene)
        m_scene->applyBoardSize(m_after);
}

ItemStackCommand::ItemStackCommand(BoardScene* scene, const ItemStackList& before,
                                   const ItemStackList& after, const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void ItemStackCommand::undo()
{
    if (m_scene)
        m_scene->applyItemStack(m_before);
}

void ItemStackCommand::redo()
{
    if (m_scene)
        m_scene->applyItemStack(m_after);
}

TextItemCommand::TextItemCommand(BoardScene* scene, TextItem* item,
                                 const TextItemState& before, const TextItemState& after,
                                 const QString& text)
    : m_scene(scene)
    , m_item(item)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void TextItemCommand::undo()
{
    if (m_scene && m_item)
        m_scene->applyTextState(m_item, m_before);
}

void TextItemCommand::redo()
{
    if (m_scene && m_item)
        m_scene->applyTextState(m_item, m_after);
}

LayerStateCommand::LayerStateCommand(BoardScene* scene, const LayerStackState& before,
                                     const LayerStackState& after, const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void LayerStateCommand::undo()
{
    if (m_scene)
        m_scene->applyLayerState(m_before);
}

void LayerStateCommand::redo()
{
    if (m_scene)
        m_scene->applyLayerState(m_after);
}
