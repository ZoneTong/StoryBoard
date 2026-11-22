#include "scene/Commands.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonItem.h"
#include "scene/CanvasScene.h"

#include <QGraphicsScene>
#include <utility>

namespace {

bool SameStack(const ItemStackList& first, const ItemStackList& second)
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
                                 const QList<qreal>& after, const QString& text, const ItemGeometryList& geometryBefore,
                                 const ItemGeometryList& geometryAfter)
    : m_scene(scene), m_before(before), m_after(after), m_geometryBefore(geometryBefore), m_geometryAfter(geometryAfter)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void RotationCommand::undo()
{
    Apply(m_before, m_geometryBefore);
}

void RotationCommand::redo()
{
    Apply(m_after, m_geometryAfter);
}

void RotationCommand::Apply(const QList<qreal>& angles, const ItemGeometryList& geometry)
{
    for (const ItemGeometry& entry : geometry) {
        if (!entry.item) {
            continue;
        }
        entry.item->setPos(entry.pos);
        entry.item->SetContentSize(entry.size);
    }
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
    if (m_scene) {
        m_namesBefore = m_scene->GroupNames();
        m_parentsBefore = m_scene->GroupParents();
        m_namesAfter = m_namesBefore;
        m_parentsAfter = m_parentsBefore;
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
    // 撤销删除时把组名 / 父子关系与空组状态一并恢复（§7.4、D17）。
    m_scene->ApplyGroupRecords(m_namesBefore, m_parentsBefore);
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
    // 删除后清理因此变空的组，避免保存后留下孤儿组记录。
    m_scene->PruneEmptyGroups();
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
    return m_mergeId.isEmpty() ? -1 : GEOMETRY_NUDGE_COMMAND_ID;
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
                           const QHash<int, int>& parentsAfter, const QHash<int, QString>& namesBefore,
                           const QHash<int, QString>& namesAfter)
    : m_scene(scene), m_before(before), m_after(after), m_parentsBefore(parentsBefore), m_parentsAfter(parentsAfter),
      m_namesBefore(namesBefore), m_namesAfter(namesAfter)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void GroupCommand::undo()
{
    Apply(m_before, m_parentsBefore, m_namesBefore);
}

void GroupCommand::redo()
{
    Apply(m_after, m_parentsAfter, m_namesAfter);
}

void GroupCommand::Apply(const QList<int>& groupIds, const QHash<int, int>& parents, const QHash<int, QString>& names)
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
    m_scene->ApplyGroupRecords(names, parents);
}

GroupRenameCommand::GroupRenameCommand(CanvasScene* scene, int groupId, const QString& before, const QString& after,
                                       const QString& text)
    : m_scene(scene), m_groupId(groupId), m_before(before), m_after(after)
{
    setText(text);
}

void GroupRenameCommand::undo()
{
    Apply(m_before);
}

void GroupRenameCommand::redo()
{
    Apply(m_after);
}

void GroupRenameCommand::Apply(const QString& name)
{
    if (!m_scene || m_groupId <= 0) {
        return;
    }
    QHash<int, QString> names = m_scene->GroupNames();
    if (name.trimmed().isEmpty()) {
        names.remove(m_groupId);
    } else {
        names.insert(m_groupId, name);
    }
    m_scene->ApplyGroupNames(names);
}

StrokeSmoothCommand::StrokeSmoothCommand(CanvasScene* scene, const QList<LineAbstractItem*>& items,
                                         const QList<LineAbstractItem::GeometryState>& before,
                                         const QList<LineAbstractItem::GeometryState>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (LineAbstractItem* item : items) {
        m_items.append(QPointer<LineAbstractItem>(item));
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

void StrokeSmoothCommand::Apply(const QList<LineAbstractItem::GeometryState>& states)
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

TextItemCommand::TextItemCommand(CanvasScene* scene, TextItem* item, const TextItemState& before,
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
    if (!SameStack(stack, other.stack) || pos != other.pos || size != other.size ||
        !qFuzzyCompare(rotation, other.rotation) || !qFuzzyCompare(1.0 + opacity, 1.0 + other.opacity) ||
        itemName != other.itemName || hasShapeState != other.hasShapeState || hasStrokeStyle != other.hasStrokeStyle ||
        textParams != other.textParams) {
        return false;
    }
    if (hasShapeState && shapeState != other.shapeState) {
        return false;
    }
    if (hasStrokeStyle && (strokeColor != other.strokeColor || !qFuzzyCompare(strokeWidth, other.strokeWidth) ||
                           strokeStyle != other.strokeStyle || curveStartArrow != other.curveStartArrow ||
                           curveEndArrow != other.curveEndArrow)) {
        return false;
    }
    if (hasArcAxes != other.hasArcAxes || curveParams != other.curveParams) {
        return false;
    }
    if (hasArcAxes && (!qFuzzyCompare(arcMajor, other.arcMajor) || !qFuzzyCompare(arcMinor, other.arcMinor))) {
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
        if (auto* shape = dynamic_cast<ShapeAbstractItem*>(m_item.data())) {
            shape->ApplyState(state.shapeState);
        }
    } else if (!state.textParams.isEmpty()) {
        // 文字属性走属性协议：正文 / 字体会重算自然尺寸，之后再按快照恢复位置与尺寸。
        for (auto it = state.textParams.constBegin(); it != state.textParams.constEnd(); ++it) {
            m_item->SetPropertyValue(it.key(), it.value());
        }
        m_item->setPos(state.pos);
        m_item->SetContentSize(state.size);
    } else {
        m_item->setPos(state.pos);
        m_item->SetContentSize(state.size);
    }

    // Rotation is a generic CanvasItem property for every item kind.
    m_item->SetRotationAngle(state.rotation);
    m_item->SetItemOpacity(state.opacity);

    if (state.hasStrokeStyle) {
        if (auto* stroke = dynamic_cast<LineAbstractItem*>(m_item.data())) {
            stroke->SetStrokeColor(state.strokeColor);
            stroke->SetStrokeWidth(state.strokeWidth);
            stroke->SetStrokePattern(state.strokeStyle);
            // 需求 27：曲线两端箭头随同一条撤销记录一起应用。
            stroke->SetArrowStyles(state.curveStartArrow, state.curveEndArrow);
            // 弧线角度 / 正弦周期振幅：按属性键回放，与面板 / 拖拽共用同一套属性写入。
            for (auto it = state.curveParams.constBegin(); it != state.curveParams.constEnd(); ++it) {
                m_item->SetPropertyValue(it.key(), it.value());
            }
            // 弧线长短半轴：走通用属性写入，保持椭圆中心重采样。
            if (state.hasArcAxes) {
                m_item->SetPropertyValue(ItemProperty::MAJOR_AXIS, state.arcMajor);
                m_item->SetPropertyValue(ItemProperty::MINOR_AXIS, state.arcMinor);
            }
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
