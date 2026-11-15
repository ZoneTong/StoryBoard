#include "Commands.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "ImageItem.h"

#include <QGraphicsScene>
#include <utility>

namespace {

bool sameStack(const ItemStackList& first, const ItemStackList& second)
{
    if (first.size() != second.size()) {
        return false;
    }
    for (int i = 0; i < first.size(); ++i) {
        if (first.at(i).item != second.at(i).item || first.at(i).layerId != second.at(i).layerId ||
            first.at(i).height != second.at(i).height) {
            return false;
        }
    }
    return true;
}

} // namespace

RotationCommand::RotationCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<qreal>& before,
                                 const QList<qreal>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void RotationCommand::undo()
{
    apply(m_before);
}

void RotationCommand::redo()
{
    apply(m_after);
}

void RotationCommand::apply(const QList<qreal>& angles)
{
    const int count = qMin(m_items.size(), angles.size());
    for (int i = 0; i < count; ++i) {
        if (m_items.at(i)) {
            m_items.at(i)->setRotationAngle(angles.at(i));
        }
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}

AddItemCommand::AddItemCommand(CanvasScene* scene, CanvasItem* item, const QString& text) : m_scene(scene), m_item(item)
{
    setText(text);
}

void AddItemCommand::undo()
{
    if (m_scene && m_item) {
        m_scene->detachItem(m_item);
    }
}

void AddItemCommand::redo()
{
    if (m_scene && m_item) {
        m_scene->attachItem(m_item);
    }
}

RemoveItemsCommand::RemoveItemsCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QString& text)
    : m_scene(scene)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void RemoveItemsCommand::undo()
{
    if (!m_scene) {
        return;
    }

    for (const QPointer<CanvasItem>& item : std::as_const(m_items)) {
        if (!item) {
            continue;
        }
        m_scene->attachItem(item);
        if (m_scene->isItemEditable(item)) {
            item->setSelected(true);
        }
    }
}

void RemoveItemsCommand::redo()
{
    if (!m_scene) {
        return;
    }

    for (const QPointer<CanvasItem>& item : std::as_const(m_items)) {
        if (item) {
            m_scene->detachItem(item);
        }
    }
}

GeometryCommand::GeometryCommand(CanvasScene* scene, const ItemGeometryList& before, const ItemGeometryList& after,
                                 const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
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
        if (!entry.item) {
            continue;
        }
        entry.item->setPos(entry.pos);
        entry.item->setContentSize(entry.size);
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}

GroupCommand::GroupCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<int>& before,
                           const QList<int>& after, const QString& text, const QHash<int, int>& parentsBefore,
                           const QHash<int, int>& parentsAfter)
    : m_scene(scene), m_before(before), m_after(after), m_parentsBefore(parentsBefore), m_parentsAfter(parentsAfter)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void GroupCommand::undo()
{
    apply(m_before, m_parentsBefore);
}

void GroupCommand::redo()
{
    apply(m_after, m_parentsAfter);
}

void GroupCommand::apply(const QList<int>& groupIds, const QHash<int, int>& parents)
{
    if (!m_scene) {
        return;
    }
    QList<CanvasItem*> items;
    items.reserve(m_items.size());
    for (const QPointer<CanvasItem>& item : std::as_const(m_items)) {
        if (item) {
            items.append(item.data());
        }
    }
    m_scene->applyGroupIds(items, groupIds);
    m_scene->applyGroupParents(parents);
}

StrokeSmoothCommand::StrokeSmoothCommand(CanvasScene* scene, const QList<CurveItem*>& items,
                                         const QList<CurveItem::GeometryState>& before,
                                         const QList<CurveItem::GeometryState>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (CurveItem* item : items) {
        m_items.append(QPointer<CurveItem>(item));
    }
}

void StrokeSmoothCommand::undo()
{
    apply(m_before);
}

void StrokeSmoothCommand::redo()
{
    apply(m_after);
}

void StrokeSmoothCommand::apply(const QList<CurveItem::GeometryState>& states)
{
    if (states.size() != m_items.size()) {
        return;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i)) {
            m_items.at(i)->applyGeometryState(states.at(i));
        }
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}

BackgroundCommand::BackgroundCommand(CanvasScene* scene, const BackgroundState& before, const BackgroundState& after,
                                     const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
}

void BackgroundCommand::undo()
{
    if (m_scene) {
        m_scene->applyBackground(m_before);
    }
}

void BackgroundCommand::redo()
{
    if (m_scene) {
        m_scene->applyBackground(m_after);
    }
}

CanvasSizeCommand::CanvasSizeCommand(CanvasScene* scene, const QRectF& before, const QRectF& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
}

void CanvasSizeCommand::undo()
{
    if (m_scene) {
        m_scene->applyCanvasRect(m_before);
    }
}

void CanvasSizeCommand::redo()
{
    if (m_scene) {
        m_scene->applyCanvasRect(m_after);
    }
}

ItemStackCommand::ItemStackCommand(CanvasScene* scene, const ItemStackList& before, const ItemStackList& after,
                                   const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
}

void ItemStackCommand::undo()
{
    if (m_scene) {
        m_scene->applyItemStack(m_before);
    }
}

void ItemStackCommand::redo()
{
    if (m_scene) {
        m_scene->applyItemStack(m_after);
    }
}

TextItemCommand::TextItemCommand(CanvasScene* scene, TextItem* item, const TextItemState& before,
                                 const TextItemState& after, const QString& text)
    : m_scene(scene), m_item(item), m_before(before), m_after(after)
{
    setText(text);
}

void TextItemCommand::undo()
{
    if (m_scene && m_item) {
        m_scene->applyTextState(m_item, m_before);
    }
}

void TextItemCommand::redo()
{
    if (m_scene && m_item) {
        m_scene->applyTextState(m_item, m_after);
    }
}

LayerStateCommand::LayerStateCommand(CanvasScene* scene, const LayerStackState& before, const LayerStackState& after,
                                     const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
}

void LayerStateCommand::undo()
{
    if (m_scene) {
        m_scene->applyLayerState(m_before);
    }
}

void LayerStateCommand::redo()
{
    if (m_scene) {
        m_scene->applyLayerState(m_after);
    }
}

ShapeStyleCommand::ShapeStyleCommand(CanvasScene* scene, const QList<ShapeItem*>& items,
                                     const QList<ShapeItemState>& before, const QList<ShapeItemState>& after,
                                     const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (ShapeItem* item : items) {
        m_items.append(QPointer<ShapeItem>(item));
    }
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
        if (!m_items.at(i)) {
            continue;
        }
        m_items.at(i)->applyState(states.at(i));
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}

CompositeFillStyleCommand::CompositeFillStyleCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                                                     const QList<QColor>& beforeColors,
                                                     const QList<FillStyle>& beforeStyles,
                                                     const QList<QColor>& afterColors,
                                                     const QList<FillStyle>& afterStyles, const QString& text)
    : m_scene(scene), m_beforeColors(beforeColors), m_beforeStyles(beforeStyles), m_afterColors(afterColors),
      m_afterStyles(afterStyles)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void CompositeFillStyleCommand::undo()
{
    apply(m_beforeColors, m_beforeStyles);
}

void CompositeFillStyleCommand::redo()
{
    apply(m_afterColors, m_afterStyles);
}

void CompositeFillStyleCommand::apply(const QList<QColor>& colors, const QList<FillStyle>& styles)
{
    for (int i = 0; i < m_items.size() && i < colors.size() && i < styles.size(); ++i) {
        if (auto* composite = dynamic_cast<CompositeShapeItem*>(m_items.at(i).data())) {
            composite->setFill(colors.at(i), styles.at(i));
        }
    }
    if (m_scene) {
        m_scene->update();
    }
}

ItemStrokeStyleCommand::ItemStrokeStyleCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                                               const QList<QColor>& beforeColors, const QList<qreal>& beforeWidths,
                                               const QList<QColor>& afterColors, const QList<qreal>& afterWidths,
                                               const QString& text)
    : m_scene(scene), m_beforeColors(beforeColors), m_beforeWidths(beforeWidths), m_afterColors(afterColors),
      m_afterWidths(afterWidths)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void ItemStrokeStyleCommand::undo()
{
    apply(m_beforeColors, m_beforeWidths);
}

void ItemStrokeStyleCommand::redo()
{
    apply(m_afterColors, m_afterWidths);
}

void ItemStrokeStyleCommand::apply(const QList<QColor>& colors, const QList<qreal>& widths)
{
    for (int i = 0; i < m_items.size() && i < colors.size() && i < widths.size(); ++i) {
        CanvasItem* item = m_items.at(i);
        if (!item) {
            continue;
        }
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            shape->setStyle(colors.at(i), shape->fillColor(), widths.at(i));
        } else if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            stroke->setStyle(colors.at(i), widths.at(i));
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            composite->setStroke(colors.at(i), widths.at(i));
        }
    }
    if (m_scene) {
        m_scene->update();
    }
}

ItemPenStyleCommand::ItemPenStyleCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                                         const QList<Qt::PenStyle>& before, const QList<Qt::PenStyle>& after,
                                         const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void ItemPenStyleCommand::undo()
{
    apply(m_before);
}

void ItemPenStyleCommand::redo()
{
    apply(m_after);
}

void ItemPenStyleCommand::apply(const QList<Qt::PenStyle>& styles)
{
    for (int i = 0; i < m_items.size() && i < styles.size(); ++i) {
        CanvasItem* item = m_items.at(i);
        if (!item) {
            continue;
        }
        const Qt::PenStyle style = styles.at(i);
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            shape->setStrokeStyle(style);
        } else if (auto* curve = dynamic_cast<CurveItem*>(item)) {
            curve->setStrokeStyle(style);
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
            composite->setStrokeStyle(style);
        }
    }
    if (m_scene) {
        m_scene->update();
    }
}

ItemOpacityCommand::ItemOpacityCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<qreal>& before,
                                       const QList<qreal>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void ItemOpacityCommand::undo()
{
    apply(m_before);
}

void ItemOpacityCommand::redo()
{
    apply(m_after);
}

void ItemOpacityCommand::apply(const QList<qreal>& values)
{
    for (int i = 0; i < m_items.size() && i < values.size(); ++i) {
        if (CanvasItem* item = m_items.at(i)) {
            item->setItemOpacity(values.at(i));
        }
    }
    if (m_scene) {
        m_scene->update();
    }
}

bool ItemPropertiesState::operator==(const ItemPropertiesState& other) const
{
    if (!sameStack(stack, other.stack) || pos != other.pos || size != other.size ||
        !qFuzzyCompare(rotation, other.rotation) || !qFuzzyCompare(1.0 + opacity, 1.0 + other.opacity) ||
        itemName != other.itemName || hasShapeState != other.hasShapeState ||
        hasCompositeStyle != other.hasCompositeStyle || hasTextState != other.hasTextState ||
        hasStrokeStyle != other.hasStrokeStyle || hasImageState != other.hasImageState ||
        hasImageName != other.hasImageName) {
        return false;
    }
    if (hasShapeState && shapeState != other.shapeState) {
        return false;
    }
    if (hasCompositeStyle &&
        (compositeFillColor != other.compositeFillColor || compositeFillStyle != other.compositeFillStyle)) {
        return false;
    }
    if (hasTextState && textState != other.textState) {
        return false;
    }
    if (hasStrokeStyle && (strokeColor != other.strokeColor || !qFuzzyCompare(strokeWidth, other.strokeWidth) ||
                           strokeStyle != other.strokeStyle)) {
        return false;
    }
    if (hasImageState && (image.cacheKey() != other.image.cacheKey() || image.size() != other.image.size())) {
        return false;
    }
    if (hasImageName && imageName != other.imageName) {
        return false;
    }
    return true;
}

ItemPropertiesCommand::ItemPropertiesCommand(CanvasScene* scene, CanvasItem* item, const ItemPropertiesState& before,
                                             const ItemPropertiesState& after, const QString& text)
    : m_scene(scene), m_item(item), m_before(before), m_after(after)
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
    if (!m_item) {
        return;
    }

    m_item->setName(state.itemName);

    if (state.hasShapeState) {
        if (auto* shape = dynamic_cast<ShapeItem*>(m_item.data())) {
            shape->applyState(state.shapeState);
        }
    } else if (state.hasTextState) {
        if (auto* text = dynamic_cast<TextItem*>(m_item.data())) {
            text->applyState(state.textState);
        }
    } else {
        m_item->setPos(state.pos);
        m_item->setContentSize(state.size);
    }

    // Rotation is a generic CanvasItem property for every item kind.
    m_item->setRotationAngle(state.rotation);
    m_item->setItemOpacity(state.opacity);

    if (state.hasCompositeStyle) {
        if (auto* composite = dynamic_cast<CompositeShapeItem*>(m_item.data())) {
            composite->setFill(state.compositeFillColor, state.compositeFillStyle);
        }
    }

    if (state.hasStrokeStyle) {
        if (auto* stroke = dynamic_cast<CurveItem*>(m_item.data())) {
            stroke->setStyle(state.strokeColor, state.strokeWidth);
            stroke->setStrokeStyle(state.strokeStyle);
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(m_item.data())) {
            composite->setStroke(state.strokeColor, state.strokeWidth);
            composite->setStrokeStyle(state.strokeStyle);
        }
    }
    if (state.hasImageName) {
        if (auto* image = dynamic_cast<ImageItem*>(m_item.data())) {
            image->setImageName(state.imageName);
        }
    }
    if (state.hasImageState) {
        if (auto* image = dynamic_cast<ImageItem*>(m_item.data())) {
            image->setImage(state.image, state.imageName);
        }
    }

    if (m_scene && !state.stack.isEmpty()) {
        m_scene->applyItemStack(state.stack);
    }
    if (m_scene) {
        m_scene->updateSceneExtent();
        m_scene->update();
    }
}
