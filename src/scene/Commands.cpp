#include "scene/Commands.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonItem.h"
#include "scene/CanvasScene.h"

#include <QColor>
#include <QGraphicsScene>
#include <QImage>
#include <QStringList>
#include <QtMath>
#include <utility>

namespace {

QString LogNumber(qreal value)
{
    return QString::number(value, 'f', 1);
}

QString PropertyLabel(PropertyKey key)
{
    if (!key.IsCommon()) {
        return key.StorageKey();
    }
    switch (key.CommonProperty()) {
    case ItemProperty::TEXT_CONTENT:
        return QStringLiteral("文字");
    case ItemProperty::TEXT_COLOR:
        return QStringLiteral("文字色");
    case ItemProperty::TEXT_FONT:
        return QStringLiteral("字体");
    case ItemProperty::FILL_STYLE:
        return QStringLiteral("填充样式");
    case ItemProperty::FILL_COLOR:
        return QStringLiteral("填充色");
    case ItemProperty::FILL_IMAGE:
        return QStringLiteral("填充图");
    case ItemProperty::FILL_IMAGE_MODE:
        return QStringLiteral("图片模式");
    case ItemProperty::STROKE_STYLE:
        return QStringLiteral("描边样式");
    case ItemProperty::STROKE_COLOR:
        return QStringLiteral("描边色");
    case ItemProperty::STROKE_WIDTH:
        return QStringLiteral("线宽");
    case ItemProperty::OPACITY:
        return QStringLiteral("不透明度");
    case ItemProperty::CORNER_RADIUS:
        return QStringLiteral("圆角");
    case ItemProperty::START_ANGLE:
        return QStringLiteral("起角");
    case ItemProperty::SWEEP_ANGLE:
        return QStringLiteral("扫角");
    case ItemProperty::MAJOR_AXIS:
        return QStringLiteral("长轴");
    case ItemProperty::MINOR_AXIS:
        return QStringLiteral("短轴");
    case ItemProperty::SINE_CYCLES:
        return QStringLiteral("波数");
    case ItemProperty::SINE_AMPLITUDE:
        return QStringLiteral("振幅");
    case ItemProperty::BOTTOM_ANGLE1:
        return QStringLiteral("底角A");
    case ItemProperty::BOTTOM_ANGLE2:
        return QStringLiteral("底角B");
    case ItemProperty::PARALLELOGRAM_ANGLE:
        return QStringLiteral("内角A");
    case ItemProperty::ARROW_START:
        return QStringLiteral("起点箭头");
    case ItemProperty::ARROW_END:
        return QStringLiteral("终点箭头");
    case ItemProperty::NAME:
        return QStringLiteral("名称");
    case ItemProperty::LAYER:
        return QStringLiteral("图层");
    case ItemProperty::HEIGHT:
        return QStringLiteral("层内高度");
    case ItemProperty::POSITION:
        return QStringLiteral("位置");
    case ItemProperty::SIZE:
        return QStringLiteral("尺寸");
    case ItemProperty::ROTATION:
        return QStringLiteral("旋转");
    case ItemProperty::CENTER:
        return QStringLiteral("中心");
    }
    return key.StorageKey();
}

bool SamePropertyValue(const QVariant& oldValue, const QVariant& newValue)
{
    const int oldType = oldValue.metaType().id();
    const int newType = newValue.metaType().id();
    if (oldType == QMetaType::QPixmap && newType == QMetaType::QPixmap) {
        return oldValue.value<QPixmap>().toImage() == newValue.value<QPixmap>().toImage();
    }
    if (oldType == QMetaType::QImage && newType == QMetaType::QImage) {
        return oldValue.value<QImage>() == newValue.value<QImage>();
    }
    return oldValue == newValue;
}

QString LogPropertyValue(PropertyKey key, const QVariant& value)
{
    if (!value.isValid()) {
        return QStringLiteral("无");
    }
    const int typeId = value.metaType().id();
    if (typeId == QMetaType::QColor) {
        return value.value<QColor>().name();
    }
    if (typeId == QMetaType::QPointF) {
        const QPointF point = value.toPointF();
        return QStringLiteral("(%1,%2)").arg(LogNumber(point.x()), LogNumber(point.y()));
    }
    if (typeId == QMetaType::QSizeF) {
        const QSizeF size = value.toSizeF();
        return QStringLiteral("%1×%2").arg(LogNumber(size.width()), LogNumber(size.height()));
    }
    if (typeId == QMetaType::QPixmap || typeId == QMetaType::QImage) {
        const QImage image = typeId == QMetaType::QPixmap ? value.value<QPixmap>().toImage() : value.value<QImage>();
        if (image.isNull()) {
            return QStringLiteral("无图");
        }
        return QStringLiteral("图 %1×%2").arg(image.width()).arg(image.height());
    }
    const bool angle =
        key.IsCommon() &&
        (key.CommonProperty() == ItemProperty::START_ANGLE || key.CommonProperty() == ItemProperty::SWEEP_ANGLE ||
         key.CommonProperty() == ItemProperty::BOTTOM_ANGLE1 || key.CommonProperty() == ItemProperty::BOTTOM_ANGLE2 ||
         key.CommonProperty() == ItemProperty::PARALLELOGRAM_ANGLE || key.CommonProperty() == ItemProperty::ROTATION);
    if (typeId == QMetaType::Double || typeId == QMetaType::Float) {
        return angle ? LogNumber(value.toDouble()) + QStringLiteral("°") : LogNumber(value.toDouble());
    }
    QString text = value.toString().simplified();
    if (text.size() > 24) {
        text = text.left(24) + QStringLiteral("…");
    }
    return text.isEmpty() ? QStringLiteral("空") : text;
}

QString DescribeGeometryChange(const ItemPlacementList& before, const ItemPlacementList& after)
{
    QStringList parts;
    int changed = 0;
    for (int i = 0; i < before.size() && i < after.size(); ++i) {
        const ItemPlacement& oldPlacement = before.at(i);
        const ItemPlacement& newPlacement = after.at(i);
        if (!oldPlacement.item || !newPlacement.item || oldPlacement.item != newPlacement.item) {
            continue;
        }
        QStringList itemParts;
        if (oldPlacement.pos != newPlacement.pos) {
            itemParts.append(QStringLiteral("位置 (%1,%2)→(%3,%4)")
                                 .arg(LogNumber(oldPlacement.pos.x()), LogNumber(oldPlacement.pos.y()),
                                      LogNumber(newPlacement.pos.x()), LogNumber(newPlacement.pos.y())));
        }
        if (oldPlacement.size != newPlacement.size) {
            itemParts.append(QStringLiteral("尺寸 %1×%2→%3×%4")
                                 .arg(LogNumber(oldPlacement.size.width()), LogNumber(oldPlacement.size.height()),
                                      LogNumber(newPlacement.size.width()), LogNumber(newPlacement.size.height())));
        }
        if (itemParts.isEmpty()) {
            continue;
        }
        ++changed;
        if (changed == 1) {
            parts = itemParts;
        }
    }
    if (changed > 1) {
        parts.append(QStringLiteral("等 %1 个图元").arg(changed));
    }
    return parts.join(QStringLiteral("，"));
}

} // namespace

RotationCommand::RotationCommand(CanvasScene* scene, const QList<CanvasItem*>& items, const QList<qreal>& before,
                                 const QList<qreal>& after, const QString& text,
                                 const ItemPlacementList& geometryBefore, const ItemPlacementList& geometryAfter)
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

QString RotationCommand::LogDetail(bool reverse) const
{
    const QList<qreal>& oldAngles = reverse ? m_after : m_before;
    const QList<qreal>& newAngles = reverse ? m_before : m_after;
    if (oldAngles.isEmpty() || newAngles.isEmpty()) {
        return {};
    }
    QString detail =
        QStringLiteral("旋转 %1°→%2°").arg(LogNumber(oldAngles.constFirst()), LogNumber(newAngles.constFirst()));
    if (qMax(m_items.size(), oldAngles.size()) > 1) {
        detail += QStringLiteral("（%1 个图元）").arg(qMax(m_items.size(), oldAngles.size()));
    }
    return detail;
}

void RotationCommand::Apply(const QList<qreal>& angles, const ItemPlacementList& geometry)
{
    for (const ItemPlacement& entry : geometry) {
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

ConvertItemCommand::ConvertItemCommand(CanvasScene* scene, CanvasItem* before, CanvasItem* after,
                                       const QList<TextItem*>& containerTexts, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    for (TextItem* item : containerTexts) {
        m_containerTexts.append(item);
    }
    setText(text);
}

void ConvertItemCommand::undo()
{
    Swap(m_after, m_before);
}

void ConvertItemCommand::redo()
{
    Swap(m_before, m_after);
}

void ConvertItemCommand::Swap(CanvasItem* detach, CanvasItem* attach)
{
    if (!m_scene || !attach) {
        return;
    }
    if (detach) {
        m_scene->DetachItem(detach);
    }
    m_scene->AttachItem(attach);
    // 容器文字跟着新图元的 uid 走：换类型新建了对象，旧 uid 不复活会留下指向不存在图元的文字。
    for (const QPointer<TextItem>& text : std::as_const(m_containerTexts)) {
        if (!text) {
            continue;
        }
        text->SetContainerUid(attach->Uid());
        m_scene->SyncContainerText(text);
    }
    attach->setSelected(true);
    m_scene->update();
}

LineRouteCommand::LineRouteCommand(CanvasScene* scene, StraightLineItem* line, int beforeRoute,
                                   const LineAbstractItem::GeometryState& beforeGeometry, int afterRoute,
                                   const LineAbstractItem::GeometryState& afterGeometry, const QString& text)
    : m_scene(scene), m_line(line), m_beforeRoute(beforeRoute), m_afterRoute(afterRoute),
      m_beforeGeometry(beforeGeometry), m_afterGeometry(afterGeometry)
{
    setText(text);
}

void LineRouteCommand::undo()
{
    Apply(m_beforeRoute, m_beforeGeometry);
}

void LineRouteCommand::redo()
{
    Apply(m_afterRoute, m_afterGeometry);
}

void LineRouteCommand::Apply(int route, const LineAbstractItem::GeometryState& geometry)
{
    if (!m_scene || !m_line) {
        return;
    }
    if (auto* line = dynamic_cast<StraightLineItem*>(m_line.data())) {
        line->SetLineRoute(static_cast<StraightLineItem::RouteMode>(route));
        line->ApplyGeometryState(geometry);
        m_scene->update();
    }
}

TextContainerCommand::TextContainerCommand(CanvasScene* scene, TextItem* text, const QString& beforeUid,
                                           const TextItemState& beforeState, const QString& afterUid,
                                           const TextItemState& afterState, const QString& label)
    : m_scene(scene), m_text(text), m_beforeUid(beforeUid), m_beforeState(beforeState), m_afterUid(afterUid),
      m_afterState(afterState)
{
    setText(label);
}

void TextContainerCommand::undo()
{
    Apply(m_beforeUid, m_beforeState);
}

void TextContainerCommand::redo()
{
    Apply(m_afterUid, m_afterState);
}

void TextContainerCommand::Apply(const QString& uid, const TextItemState& state)
{
    if (!m_scene || !m_text) {
        return;
    }
    m_text->SetContainerUid(uid);
    m_text->ApplyState(state);
    // 仍归属图形时按容器当前位置重排一次：撤销 / 重做期间容器可能已经被移动或缩放。
    if (!uid.isEmpty()) {
        m_scene->SyncContainerText(m_text);
    }
    m_scene->update();
}

MirrorCommand::MirrorCommand(CanvasScene* scene, const QList<MirrorItemState>& before,
                             const QList<MirrorItemState>& after, const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
}

void MirrorCommand::undo()
{
    Apply(m_before);
}

void MirrorCommand::redo()
{
    Apply(m_after);
}

void MirrorCommand::Apply(const QList<MirrorItemState>& states)
{
    for (const MirrorItemState& state : states) {
        if (!state.item) {
            continue;
        }
        state.item->setPos(state.pos);
        state.item->SetRotationAngle(state.rotation);
        state.item->SetFlips(state.flipX, state.flipY);
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
        m_groupsBefore = m_scene->GroupSnapshot();
        m_groupsAfter = m_groupsBefore;
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
    m_scene->ApplyGroupRecords(m_groupsBefore);
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

GeometryCommand::GeometryCommand(CanvasScene* scene, const ItemPlacementList& before, const ItemPlacementList& after,
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

QString GeometryCommand::LogDetail(bool reverse) const
{
    const ItemPlacementList& oldPlacements = reverse ? m_after : m_before;
    const ItemPlacementList& newPlacements = reverse ? m_before : m_after;
    return DescribeGeometryChange(oldPlacements, newPlacements);
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

void GeometryCommand::Apply(const ItemPlacementList& geometry)
{
    for (const ItemPlacement& entry : geometry) {
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
                           const QList<int>& after, const QString& text, const GroupState& groupsBefore,
                           const GroupState& groupsAfter)
    : m_scene(scene), m_before(before), m_after(after), m_groupsBefore(groupsBefore), m_groupsAfter(groupsAfter)
{
    setText(text);
    for (CanvasItem* item : items) {
        m_items.append(QPointer<CanvasItem>(item));
    }
}

void GroupCommand::undo()
{
    Apply(m_before, m_groupsBefore);
}

void GroupCommand::redo()
{
    Apply(m_after, m_groupsAfter);
}

void GroupCommand::Apply(const QList<int>& groupIds, const GroupState& groups)
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
    m_scene->ApplyGroupRecords(groups);
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

ArrowBindingCommand::ArrowBindingCommand(CanvasScene* scene, const QList<LineAbstractItem*>& items,
                                         const QList<ArrowBindingState>& before, const QList<ArrowBindingState>& after,
                                         const QString& text)
    : m_scene(scene), m_before(before), m_after(after)
{
    setText(text);
    for (LineAbstractItem* item : items) {
        m_items.append(QPointer<LineAbstractItem>(item));
    }
}

void ArrowBindingCommand::undo()
{
    Apply(m_before);
}

void ArrowBindingCommand::redo()
{
    Apply(m_after);
}

void ArrowBindingCommand::Apply(const QList<ArrowBindingState>& states)
{
    if (states.size() != m_items.size()) {
        return;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        LineAbstractItem* item = m_items.at(i);
        if (!item) {
            continue;
        }
        item->SetStartBinding(states.at(i).start);
        item->SetEndBinding(states.at(i).end);
        item->update();
    }
    if (m_scene) {
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
                                         const QList<QMap<PropertyKey, QVariant>>& before,
                                         const QList<QMap<PropertyKey, QVariant>>& after, const QString& text)
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

QString ItemPropertyCommand::LogDetail(bool reverse) const
{
    if (m_before.isEmpty() || m_after.isEmpty()) {
        return {};
    }
    const QMap<PropertyKey, QVariant>& oldValues = reverse ? m_after.constFirst() : m_before.constFirst();
    const QMap<PropertyKey, QVariant>& newValues = reverse ? m_before.constFirst() : m_after.constFirst();
    QStringList parts;
    for (auto it = newValues.constBegin(); it != newValues.constEnd(); ++it) {
        const auto oldIt = oldValues.constFind(it.key());
        const QVariant oldValue = oldIt == oldValues.constEnd() ? QVariant() : oldIt.value();
        if (oldIt != oldValues.constEnd() && SamePropertyValue(oldValue, it.value())) {
            continue;
        }
        parts.append(QStringLiteral("%1 %2→%3")
                         .arg(PropertyLabel(it.key()), LogPropertyValue(it.key(), oldValue),
                              LogPropertyValue(it.key(), it.value())));
    }
    if (m_items.size() > 1) {
        parts.append(QStringLiteral("等 %1 个图元").arg(m_items.size()));
    }
    return parts.join(QStringLiteral("，"));
}

void ItemPropertyCommand::Apply(const QList<QMap<PropertyKey, QVariant>>& states)
{
    // 先换层、再定层内高度，最后写位置/尺寸与其余属性；避免换层时先按旧层归一化高度。
    // 文字正文会重算自然尺寸，先写正文/字体，再回放位置与尺寸。
    static const QList<PropertyKey> ORDER = {
        ItemProperty::LAYER,      ItemProperty::HEIGHT,   ItemProperty::TEXT_CONTENT, ItemProperty::TEXT_FONT,
        ItemProperty::TEXT_COLOR, ItemProperty::POSITION, ItemProperty::SIZE,         ItemProperty::ROTATION};
    ItemStackList stack;
    for (int index = 0; index < m_items.size() && index < states.size(); ++index) {
        CanvasItem* item = m_items.at(index);
        if (!item) {
            continue;
        }
        const QMap<PropertyKey, QVariant>& values = states.at(index);
        for (const PropertyKey& property : ORDER) {
            const auto it = values.constFind(property);
            if (it != values.constEnd()) {
                item->SetPropertyValueByKey(property, it.value());
            }
        }
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!ORDER.contains(it.key())) {
                item->SetPropertyValueByKey(it.key(), it.value());
            }
        }
        if (values.contains(ItemProperty::LAYER) && values.contains(ItemProperty::HEIGHT)) {
            stack.append({item, item->LayerId(), item->ZOrder()});
        }
    }
    if (!m_scene) {
        return;
    }
    if (stack.isEmpty()) {
        m_scene->update();
    } else {
        m_scene->ApplyItemStack(stack);
    }
}
