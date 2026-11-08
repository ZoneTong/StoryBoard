#include "Commands.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "ImageItem.h"
#include "StrokeItem.h"

#include <QGraphicsScene>

#include <utility>

namespace {

bool sameStack(const ItemStackList& first, const ItemStackList& second)
{
    if (first.size() != second.size())
        return false;
    for (int i = 0; i < first.size(); ++i) {
        if (first.at(i).item != second.at(i).item
            || first.at(i).layerId != second.at(i).layerId
            || first.at(i).height != second.at(i).height) {
            return false;
        }
    }
    return true;
}

} // namespace

AddItemCommand::AddItemCommand(CanvasScene* scene, CanvasItem* item, const QString& text)
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

RemoveItemsCommand::RemoveItemsCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                                       const QString& text)
    : m_scene(scene)
{
    setText(text);
    for (CanvasItem* item : items)
        m_items.append(QPointer<CanvasItem>(item));
}

void RemoveItemsCommand::undo()
{
    if (!m_scene)
        return;

    for (const QPointer<CanvasItem>& item : std::as_const(m_items)) {
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

    for (const QPointer<CanvasItem>& item : std::as_const(m_items)) {
        if (item)
            m_scene->detachItem(item);
    }
}

GeometryCommand::GeometryCommand(CanvasScene* scene, const ItemGeometryList& before,
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

BackgroundCommand::BackgroundCommand(CanvasScene* scene, const BackgroundState& before,
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

CanvasSizeCommand::CanvasSizeCommand(CanvasScene* scene, const QRectF& before, const QRectF& after,
                                   const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void CanvasSizeCommand::undo()
{
    if (m_scene)
        m_scene->applyCanvasRect(m_before);
}

void CanvasSizeCommand::redo()
{
    if (m_scene)
        m_scene->applyCanvasRect(m_after);
}

ItemStackCommand::ItemStackCommand(CanvasScene* scene, const ItemStackList& before,
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

TextItemCommand::TextItemCommand(CanvasScene* scene, TextItem* item,
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

LayerStateCommand::LayerStateCommand(CanvasScene* scene, const LayerStackState& before,
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

ShapeStyleCommand::ShapeStyleCommand(CanvasScene* scene, const QList<ShapeItem*>& items,
                                     const QList<ShapeItemState>& before,
                                     const QList<ShapeItemState>& after,
                                     const QString& text)
    : m_scene(scene)
    , m_before(before)
    , m_after(after)
{
    setText(text);
    for (ShapeItem* item : items)
        m_items.append(QPointer<ShapeItem>(item));
}

void ShapeStyleCommand::undo()
{
    apply(m_before);
}

void ShapeStyleCommand::redo()
{
    apply(m_after);
}

void ShapeStyleCommand::apply(const QList<ShapeItemState>& states)
{
    for (int i = 0; i < m_items.size() && i < states.size(); ++i) {
        if (!m_items.at(i))
            continue;
        m_items.at(i)->applyState(states.at(i));
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}

bool ItemPropertiesState::operator==(const ItemPropertiesState& other) const
{
    if (!sameStack(stack, other.stack)
        || pos != other.pos
        || size != other.size
        || !qFuzzyCompare(rotation, other.rotation)
        || itemName != other.itemName
        || hasShapeState != other.hasShapeState
        || hasTextState != other.hasTextState
        || hasStrokeStyle != other.hasStrokeStyle
        || hasImageName != other.hasImageName) {
        return false;
    }
    if (hasShapeState && shapeState != other.shapeState)
        return false;
    if (hasTextState && textState != other.textState)
        return false;
    if (hasStrokeStyle
        && (strokeColor != other.strokeColor
            || !qFuzzyCompare(strokeWidth, other.strokeWidth))) {
        return false;
    }
    if (hasImageName && imageName != other.imageName)
        return false;
    return true;
}

ItemPropertiesCommand::ItemPropertiesCommand(CanvasScene* scene, CanvasItem* item,
                                             const ItemPropertiesState& before,
                                             const ItemPropertiesState& after,
                                             const QString& text)
    : m_scene(scene)
    , m_item(item)
    , m_before(before)
    , m_after(after)
{
    setText(text);
}

void ItemPropertiesCommand::undo()
{
    apply(m_before);
}

void ItemPropertiesCommand::redo()
{
    apply(m_after);
}

void ItemPropertiesCommand::apply(const ItemPropertiesState& state)
{
    if (!m_item)
        return;

    m_item->setName(state.itemName);

    if (state.hasShapeState) {
        if (auto* shape = dynamic_cast<ShapeItem*>(m_item.data()))
            shape->applyState(state.shapeState);
    } else if (state.hasTextState) {
        if (auto* text = dynamic_cast<TextItem*>(m_item.data()))
            text->applyState(state.textState);
    } else {
        m_item->setPos(state.pos);
        m_item->setContentSize(state.size);
    }

    // Rotation is a generic CanvasItem property for every item kind.
    m_item->setRotationAngle(state.rotation);

    if (state.hasStrokeStyle) {
        if (auto* stroke = dynamic_cast<StrokeItem*>(m_item.data()))
            stroke->setStyle(state.strokeColor, state.strokeWidth);
    }
    if (state.hasImageName) {
        if (auto* image = dynamic_cast<ImageItem*>(m_item.data()))
            image->setImageName(state.imageName);
    }

    if (m_scene && !state.stack.isEmpty())
        m_scene->applyItemStack(state.stack);
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}
