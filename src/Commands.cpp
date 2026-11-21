#include "Commands.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "LineItemAbstract.h"
#include "PolygonItem.h"

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
    Apply(m_before);
}

void RotationCommand::redo()
{
    Apply(m_after);
}

void RotationCommand::Apply(const QList<qreal>& angles)
{
    const int count = qMin(m_items.size(), angles.size());
    for (int i = 0; i < count; ++i) {
        if (m_items.at(i)) {
            m_items.at(i)->SetRotationAngle(angles.at(i));
        }
    }
    if (m_scene) {
        m_scene->UpdateSceneExtent();
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
        m_scene->DetachItem(m_item);
    }
}

void AddItemCommand::redo()
{
    if (m_scene && m_item) {
        m_scene->AttachItem(m_item);
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
        m_scene->AttachItem(item);
        if (m_scene->IsItemEditable(item)) {
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
            m_scene->DetachItem(item);
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
    Apply(m_before);
}

void GeometryCommand::redo()
{
    Apply(m_after);
}

void GeometryCommand::SetMergeId(const QString& id)
{
    m_mergeId = id;
}

int GeometryCommand::id() const
{
    return m_mergeId.isEmpty() ? -1 : GeometryNudgeCommandId;
}

bool GeometryCommand::mergeWith(const QUndoCommand* other)
{
    // 合并条件：同为 nudge 标记，且作用的是同一批图元（指针与顺序一致）。
    // 满足时新命令并入旧命令：撤销回到最早的 before，重做到最新的 after。
    const auto* geometry = dynamic_cast<const GeometryCommand*>(other);
    if (!geometry || geometry->m_mergeId.isEmpty() || geometry->m_mergeId != m_mergeId) {
        return false;
    }
    if (geometry->m_before.size() != m_before.size() || geometry->m_after.size() != m_after.size()) {
        return false;
    }
    for (int i = 0; i < m_before.size(); ++i) {
        if (geometry->m_before.at(i).item != m_before.at(i).item ||
            geometry->m_after.at(i).item != m_after.at(i).item) {
            return false;
        }
    }
    m_before = geometry->m_before;
    setText(geometry->text());
    return true;
}

void GeometryCommand::Apply(const ItemGeometryList& geometry)
{
    for (const ItemGeometry& entry : geometry) {
        if (!entry.item) {
            continue;
        }
        entry.item->setPos(entry.pos);
        entry.item->SetContentSize(entry.size);
    }
    if (m_scene) {
        m_scene->UpdateSceneExtent();
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
    Apply(m_before, m_parentsBefore);
}

void GroupCommand::redo()
{
    Apply(m_after, m_parentsAfter);
}

void GroupCommand::Apply(const QList<int>& groupIds, const QHash<int, int>& parents)
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
    m_scene->ApplyGroupIds(items, groupIds);
    m_scene->ApplyGroupParents(parents);
}

StrokeSmoothCommand::StrokeSmoothCommand(CanvasScene* scene, const QList<LineItemAbstract*>& items,
                                         const QList<LineItemAbstract::GeometryState>& before,
                                         const QList<LineItemAbstract::GeometryState>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (LineItemAbstract* item : items) {
        m_items.append(QPointer<LineItemAbstract>(item));
    }
}

void StrokeSmoothCommand::undo()
{
    Apply(m_before);
}

void StrokeSmoothCommand::redo()
{
    Apply(m_after);
}

void StrokeSmoothCommand::Apply(const QList<LineItemAbstract::GeometryState>& states)
{
    if (states.size() != m_items.size()) {
        return;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i)) {
            m_items.at(i)->ApplyGeometryState(states.at(i));
        }
    }
    if (m_scene) {
        m_scene->UpdateSceneExtent();
        m_scene->update();
    }
}

BackgroundCommand::BackgroundCommand(CanvasScene* scene, const CanvasBackground& before, const CanvasBackground& after,
                                     const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
}

void BackgroundCommand::undo()
{
    if (m_scene) {
        m_scene->SetCanvasBackground(m_before);
    }
}

void BackgroundCommand::redo()
{
    if (m_scene) {
        m_scene->SetCanvasBackground(m_after);
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
        m_scene->ApplyCanvasRect(m_before);
    }
}

void CanvasSizeCommand::redo()
{
    if (m_scene) {
        m_scene->ApplyCanvasRect(m_after);
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
        m_scene->ApplyItemStack(m_before);
    }
}

void ItemStackCommand::redo()
{
    if (m_scene) {
        m_scene->ApplyItemStack(m_after);
    }
}

TextItemCommand::TextItemCommand(CanvasScene* scene, CanvasItem* item, const TextItemState& before,
                                 const TextItemState& after, const QString& text)
    : m_scene(scene), m_item(item), m_before(before), m_after(after)
{
    setText(text);
}

void TextItemCommand::undo()
{
    if (m_scene && m_item) {
        m_scene->ApplyTextState(m_item, m_before);
    }
}

void TextItemCommand::redo()
{
    if (m_scene && m_item) {
        m_scene->ApplyTextState(m_item, m_after);
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
        m_scene->ApplyLayerState(m_before);
    }
}

void LayerStateCommand::redo()
{
    if (m_scene) {
        m_scene->ApplyLayerState(m_after);
    }
}

// 透明度对图形、笔画、文字、图片等所有图元都生效。
// 属性表命令：一次编辑把同一组属性值写回所有目标图元，只占一条撤销记录。
// 替代原先按图元类型分拆的样式命令，读写都走 Properties()/PropertyValue()/SetPropertyValue()。
ItemPropertyCommand::ItemPropertyCommand(CanvasScene* scene, const QList<CanvasItem*>& items,
                                         const QList<QMap<ItemProperty, QVariant>>& before,
                                         const QList<QMap<ItemProperty, QVariant>>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void ItemPropertyCommand::undo()
{
    Apply(m_before);
}

void ItemPropertyCommand::redo()
{
    Apply(m_after);
}

void ItemPropertyCommand::Apply(const QList<QMap<ItemProperty, QVariant>>& states)
{
    for (int index = 0; index < m_items.size() && index < states.size(); ++index) {
        CanvasItem* item = m_items.at(index);
        if (!item) {
            continue;
        }
        const QMap<ItemProperty, QVariant>& values = states.at(index);
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            item->SetPropertyValue(it.key(), it.value());
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
        itemName != other.itemName || hasShapeState != other.hasShapeState || hasTextState != other.hasTextState ||
        hasStrokeStyle != other.hasStrokeStyle) {
        return false;
    }
    if (hasShapeState && shapeState != other.shapeState) {
        return false;
    }
    if (hasTextState && textState != other.textState) {
        return false;
    }
    if (hasStrokeStyle && (strokeColor != other.strokeColor || !qFuzzyCompare(strokeWidth, other.strokeWidth) ||
                           strokeStyle != other.strokeStyle || curveStartArrow != other.curveStartArrow ||
                           curveEndArrow != other.curveEndArrow)) {
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
    Apply(m_before);
}

void ItemPropertiesCommand::redo()
{
    Apply(m_after);
}

void ItemPropertiesCommand::Apply(const ItemPropertiesState& state)
{
    if (!m_item) {
        return;
    }

    m_item->SetName(state.itemName);

    if (state.hasShapeState) {
        if (auto* shape = dynamic_cast<ShapeItem*>(m_item.data())) {
            shape->ApplyState(state.shapeState);
        }
    } else if (state.hasTextState) {
        m_item->ApplyTextState(state.textState);
    } else {
        m_item->setPos(state.pos);
        m_item->SetContentSize(state.size);
    }

    // Rotation is a generic CanvasItem property for every item kind.
    m_item->SetRotationAngle(state.rotation);
    m_item->SetItemOpacity(state.opacity);

    if (state.hasStrokeStyle) {
        if (auto* stroke = dynamic_cast<LineItemAbstract*>(m_item.data())) {
            stroke->SetStrokeColor(state.strokeColor);
            stroke->SetStrokeWidth(state.strokeWidth);
            stroke->SetStrokePattern(state.strokeStyle);
            // 需求 27：曲线两端箭头随同一条撤销记录一起应用。
            stroke->SetArrowStyles(state.curveStartArrow, state.curveEndArrow);
        }
    }

    if (m_scene && !state.stack.isEmpty()) {
        m_scene->ApplyItemStack(state.stack);
    }
    if (m_scene) {
        m_scene->UpdateSceneExtent();
        m_scene->update();
    }
}
