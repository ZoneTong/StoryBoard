#include "scene/Commands.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/lines/StraightLineItem.h"
#include "items/polygon/PolygonItem.h"
#include "scene/CanvasScene.h"

#include <QColor>
#include <QFont>
#include <QGraphicsScene>
#include <QImage>
#include <QLineF>
#include <QSet>
#include <QStringList>
#include <QtMath>
#include <algorithm>
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

QString LogColor(const QColor& color)
{
    if (!color.isValid()) {
        return QStringLiteral("透明");
    }
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

QString LogStrokeStyle(int style)
{
    switch (static_cast<Qt::PenStyle>(style)) {
    case Qt::NoPen:
        return QStringLiteral("无描边");
    case Qt::SolidLine:
        return QStringLiteral("实线");
    case Qt::DashLine:
        return QStringLiteral("虚线");
    case Qt::DotLine:
        return QStringLiteral("点线");
    case Qt::DashDotLine:
        return QStringLiteral("点划线");
    case Qt::DashDotDotLine:
        return QStringLiteral("双点划线");
    case Qt::CustomDashLine:
        return QStringLiteral("自定义虚线");
    }
    return QStringLiteral("线型 %1").arg(style);
}

QString LogFillStyle(int style)
{
    const FillStyle fillStyle = static_cast<FillStyle>(style);
    return FillStyles::Name(fillStyle);
}

QString LogImageMode(int mode)
{
    switch (static_cast<ImageFill::Mode>(mode)) {
    case ImageFill::Mode::FIT:
        return QStringLiteral("适应");
    case ImageFill::Mode::TILE:
        return QStringLiteral("平铺");
    case ImageFill::Mode::STRETCH:
        return QStringLiteral("拉伸");
    }
    return QStringLiteral("模式 %1").arg(mode);
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
        return LogColor(value.value<QColor>());
    }
    if (key.IsCommon()) {
        switch (key.CommonProperty()) {
        case ItemProperty::FILL_STYLE:
            return LogFillStyle(value.toInt());
        case ItemProperty::STROKE_STYLE:
            return LogStrokeStyle(value.toInt());
        case ItemProperty::FILL_IMAGE_MODE:
            return LogImageMode(value.toInt());
        case ItemProperty::OPACITY:
            return QStringLiteral("%1%").arg(qRound(value.toDouble() * 100.0));
        default:
            break;
        }
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
    if (key.IsCommon() &&
        (key.CommonProperty() == ItemProperty::ARROW_START || key.CommonProperty() == ItemProperty::ARROW_END)) {
        return ArrowStyles::Name(static_cast<ArrowStyle>(value.toInt()));
    }
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
                                         const QList<LineAbstractItem::GeometryState>& after, const QString& text,
                                         const QList<QPointF>& beforeKnots, const QList<QPointF>& afterKnots)
    : m_scene(scene), m_before(before), m_after(after), m_beforeKnots(beforeKnots), m_afterKnots(afterKnots)
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
    QStringList parts;
    const int count = qMin(m_before.size(), m_after.size());
    for (int index = 0; index < count; ++index) {
        const QMap<PropertyKey, QVariant>& oldValues = reverse ? m_after.at(index) : m_before.at(index);
        const QMap<PropertyKey, QVariant>& newValues = reverse ? m_before.at(index) : m_after.at(index);
        for (auto it = newValues.constBegin(); it != newValues.constEnd(); ++it) {
            const auto oldIt = oldValues.constFind(it.key());
            const QVariant oldValue = oldIt == oldValues.constEnd() ? QVariant() : oldIt.value();
            if (oldIt != oldValues.constEnd() && SamePropertyValue(oldValue, it.value())) {
                continue;
            }
            const QString part = QStringLiteral("%1 %2→%3")
                                     .arg(PropertyLabel(it.key()), LogPropertyValue(it.key(), oldValue),
                                          LogPropertyValue(it.key(), it.value()));
            if (!parts.contains(part)) {
                parts.append(part);
            }
        }
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
namespace {

QString LogText(const QString& text, int limit = 24)
{
    QString value = text.simplified();
    if (value.size() > limit) {
        value = value.left(limit) + QStringLiteral("…");
    }
    return value.isEmpty() ? QStringLiteral("空") : value;
}

QVector<QPointF> GeometryKnots(const LineAbstractItem::GeometryState& state)
{
    if (state.hasDot) {
        return {state.dotCenter};
    }
    return CurveGeometry::Knots(state.segments);
}

qreal PolylineLength(const QVector<QPointF>& knots)
{
    qreal length = 0.0;
    for (int i = 1; i < knots.size(); ++i) {
        length += QLineF(knots.at(i - 1), knots.at(i)).length();
    }
    return length;
}

QString DescribeLineGeometryChange(const LineAbstractItem::GeometryState& before,
                                   const LineAbstractItem::GeometryState& after)
{
    const QVector<QPointF> oldKnots = GeometryKnots(before);
    const QVector<QPointF> newKnots = GeometryKnots(after);
    if (oldKnots.isEmpty() || newKnots.isEmpty()) {
        return {};
    }
    QStringList parts;
    if (oldKnots.size() != newKnots.size()) {
        parts.append(QStringLiteral("节点 %1→%2").arg(oldKnots.size()).arg(newKnots.size()));
    }
    if (oldKnots.size() >= 2 && newKnots.size() >= 2) {
        const qreal oldLength = PolylineLength(oldKnots);
        const qreal newLength = PolylineLength(newKnots);
        if (!qFuzzyCompare(oldLength + 1.0, newLength + 1.0)) {
            parts.append(QStringLiteral("长度 %1→%2").arg(LogNumber(oldLength), LogNumber(newLength)));
        }
        const QPointF oldLocalStart = oldKnots.constFirst();
        const QPointF oldLocalEnd = oldKnots.constLast();
        const QPointF newLocalStart = newKnots.constFirst();
        const QPointF newLocalEnd = newKnots.constLast();
        const qreal oldAngle =
            qRadiansToDegrees(qAtan2(oldLocalEnd.y() - oldLocalStart.y(), oldLocalEnd.x() - oldLocalStart.x()));
        const qreal newAngle =
            qRadiansToDegrees(qAtan2(newLocalEnd.y() - newLocalStart.y(), newLocalEnd.x() - newLocalStart.x()));
        if (!qFuzzyCompare(oldAngle + 1.0, newAngle + 1.0)) {
            parts.append(QStringLiteral("角度 %1°→%2°").arg(LogNumber(oldAngle), LogNumber(newAngle)));
        }
    }
    return parts.join(QStringLiteral("，"));
}

QString DescribeTextStateChange(const TextItemState& before, const TextItemState& after)
{
    QStringList parts;
    if (before.text != after.text) {
        parts.append(QStringLiteral("文字 \"%1\"→\"%2\"").arg(LogText(before.text), LogText(after.text)));
    }
    if (before.font != after.font) {
        const auto fontText = [](const QFont& font) {
            return QStringLiteral("%1 %2pt").arg(font.family()).arg(LogNumber(font.pointSizeF()));
        };
        parts.append(QStringLiteral("字体 %1→%2").arg(fontText(before.font), fontText(after.font)));
    }
    if (before.color != after.color) {
        parts.append(QStringLiteral("文字色 %1→%2").arg(LogColor(before.color), LogColor(after.color)));
    }
    if (before.pos != after.pos) {
        parts.append(QStringLiteral("位置 (%1,%2)→(%3,%4)")
                         .arg(LogNumber(before.pos.x()), LogNumber(before.pos.y()), LogNumber(after.pos.x()),
                              LogNumber(after.pos.y())));
    }
    if (before.size != after.size) {
        parts.append(QStringLiteral("尺寸 %1×%2→%3×%4")
                         .arg(LogNumber(before.size.width()), LogNumber(before.size.height()),
                              LogNumber(after.size.width()), LogNumber(after.size.height())));
    }
    return parts.join(QStringLiteral("，"));
}

QString BackgroundTypeLabel(BackgroundType type)
{
    switch (type) {
    case BackgroundType::NONE:
        return QStringLiteral("无");
    case BackgroundType::COLOR:
        return QStringLiteral("颜色");
    case BackgroundType::IMAGE:
        return QStringLiteral("图片");
    }
    return QStringLiteral("未知");
}

QString DescribeLayerStackChange(const LayerStackState& before, const LayerStackState& after)
{
    QStringList parts;
    QSet<int> ids;
    for (const CanvasLayer& layer : before.layers) {
        ids.insert(layer.id);
    }
    for (const CanvasLayer& layer : after.layers) {
        ids.insert(layer.id);
    }
    for (int id : ids) {
        const auto oldIt = std::find_if(before.layers.cbegin(), before.layers.cend(),
                                        [id](const CanvasLayer& layer) { return layer.id == id; });
        const auto newIt = std::find_if(after.layers.cbegin(), after.layers.cend(),
                                        [id](const CanvasLayer& layer) { return layer.id == id; });
        if (oldIt == before.layers.cend()) {
            parts.append(QStringLiteral("新增图层「%1」").arg(newIt->name));
        } else if (newIt == after.layers.cend()) {
            parts.append(QStringLiteral("删除图层「%1」").arg(oldIt->name));
        } else {
            if (oldIt->name != newIt->name) {
                parts.append(QStringLiteral("图层名 \"%1\"→\"%2\"").arg(oldIt->name, newIt->name));
            }
            if (oldIt->visible != newIt->visible) {
                parts.append(QStringLiteral("图层「%1」可见 %2→%3")
                                 .arg(newIt->name, oldIt->visible ? QStringLiteral("是") : QStringLiteral("否"),
                                      newIt->visible ? QStringLiteral("是") : QStringLiteral("否")));
            }
            if (oldIt->locked != newIt->locked) {
                parts.append(QStringLiteral("图层「%1」锁定 %2→%3")
                                 .arg(newIt->name, oldIt->locked ? QStringLiteral("是") : QStringLiteral("否"),
                                      newIt->locked ? QStringLiteral("是") : QStringLiteral("否")));
            }
        }
    }
    if (before.activeLayerId != after.activeLayerId) {
        parts.append(QStringLiteral("活动图层 %1→%2").arg(before.activeLayerId).arg(after.activeLayerId));
    }
    return parts.join(QStringLiteral("，"));
}

} // namespace

QString GroupCommand::LogDetail(bool reverse) const
{
    const QList<int>& oldIds = reverse ? m_after : m_before;
    const QList<int>& newIds = reverse ? m_before : m_after;
    const QHash<int, QString>& oldNames = reverse ? m_groupsAfter.names : m_groupsBefore.names;
    const QHash<int, QString>& newNames = reverse ? m_groupsBefore.names : m_groupsAfter.names;
    if (oldIds.isEmpty() || newIds.isEmpty()) {
        return {};
    }
    QStringList parts;
    for (int i = 0; i < oldIds.size() && i < newIds.size(); ++i) {
        if (oldIds.at(i) == newIds.at(i)) {
            continue;
        }
        const QString oldGroup =
            oldIds.at(i) == 0 ? QStringLiteral("无") : QStringLiteral("「%1」").arg(oldNames.value(oldIds.at(i)));
        const QString newGroup =
            newIds.at(i) == 0 ? QStringLiteral("无") : QStringLiteral("「%1」").arg(newNames.value(newIds.at(i)));
        parts.append(QStringLiteral("分组 %1→%2").arg(oldGroup, newGroup));
        break;
    }
    if (parts.isEmpty()) {
        return {};
    }
    if (m_items.size() > 1) {
        parts.append(QStringLiteral("等 %1 个图元").arg(m_items.size()));
    }
    return parts.join(QStringLiteral("，"));
}

QString GroupRenameCommand::LogDetail(bool reverse) const
{
    const QString oldName = reverse ? m_after : m_before;
    const QString newName = reverse ? m_before : m_after;
    return oldName == newName ? QString() : QStringLiteral("组名 \"%1\"→\"%2\"").arg(oldName, newName);
}

QString BackgroundCommand::LogDetail(bool reverse) const
{
    const CanvasBackground& before = reverse ? m_after : m_before;
    const CanvasBackground& after = reverse ? m_before : m_after;
    if (before == after) {
        return {};
    }
    QStringList parts;
    if (before.type != after.type) {
        parts.append(
            QStringLiteral("背景类型 %1→%2").arg(BackgroundTypeLabel(before.type), BackgroundTypeLabel(after.type)));
    }
    if (before.color != after.color) {
        parts.append(QStringLiteral("背景色 %1→%2").arg(LogColor(before.color), LogColor(after.color)));
    }
    if (before.mode != after.mode) {
        parts.append(QStringLiteral("背景模式 %1→%2")
                         .arg(BackgroundModeToString(before.mode), BackgroundModeToString(after.mode)));
    }
    if (before.image.isNull() != after.image.isNull() ||
        (!before.image.isNull() && before.image.toImage() != after.image.toImage())) {
        parts.append(QStringLiteral("背景图 %1→%2")
                         .arg(before.image.isNull()
                                  ? QStringLiteral("无")
                                  : QStringLiteral("%1×%2").arg(before.image.width()).arg(before.image.height()),
                              after.image.isNull()
                                  ? QStringLiteral("无")
                                  : QStringLiteral("%1×%2").arg(after.image.width()).arg(after.image.height())));
    }
    return parts.join(QStringLiteral("，"));
}

QString CanvasSizeCommand::LogDetail(bool reverse) const
{
    const QRectF before = reverse ? m_after : m_before;
    const QRectF after = reverse ? m_before : m_after;
    if (qFuzzyCompare(before.width() + 1.0, after.width() + 1.0) &&
        qFuzzyCompare(before.height() + 1.0, after.height() + 1.0)) {
        return {};
    }
    return QStringLiteral("画布尺寸 %1×%2→%3×%4")
        .arg(LogNumber(before.width()), LogNumber(before.height()), LogNumber(after.width()),
             LogNumber(after.height()));
}

QString ItemStackCommand::LogDetail(bool reverse) const
{
    const ItemStackList& before = reverse ? m_after : m_before;
    const ItemStackList& after = reverse ? m_before : m_after;
    QStringList parts;
    for (int i = 0; i < before.size() && i < after.size(); ++i) {
        if (!before.at(i).item || !after.at(i).item || before.at(i).item != after.at(i).item) {
            continue;
        }
        if (before.at(i).layerId != after.at(i).layerId) {
            parts.append(QStringLiteral("图层 %1→%2").arg(before.at(i).layerId).arg(after.at(i).layerId));
        }
        if (before.at(i).height != after.at(i).height) {
            parts.append(QStringLiteral("层内高度 %1→%2").arg(before.at(i).height).arg(after.at(i).height));
        }
    }
    if (parts.isEmpty()) {
        return {};
    }
    if (before.size() > 1) {
        parts.append(QStringLiteral("等 %1 个图元").arg(before.size()));
    }
    return parts.join(QStringLiteral("，"));
}

QString TextItemCommand::LogDetail(bool reverse) const
{
    return DescribeTextStateChange(reverse ? m_after : m_before, reverse ? m_before : m_after);
}

QString TextContainerCommand::LogDetail(bool reverse) const
{
    const QString oldUid = reverse ? m_afterUid : m_beforeUid;
    const QString newUid = reverse ? m_beforeUid : m_afterUid;
    QStringList parts;
    if (oldUid != newUid) {
        parts.append(QStringLiteral("容器 %1→%2")
                         .arg(oldUid.isEmpty() ? QStringLiteral("无") : oldUid,
                              newUid.isEmpty() ? QStringLiteral("无") : newUid));
    }
    const QString stateChange =
        DescribeTextStateChange(reverse ? m_afterState : m_beforeState, reverse ? m_beforeState : m_afterState);
    if (!stateChange.isEmpty()) {
        parts.append(stateChange);
    }
    return parts.join(QStringLiteral("，"));
}

QString ConvertItemCommand::LogDetail(bool reverse) const
{
    const CanvasItem* before = reverse ? m_after : m_before;
    const CanvasItem* after = reverse ? m_before : m_after;
    if (!before || !after || before->Kind() == after->Kind()) {
        return {};
    }
    return QStringLiteral("类型 %1→%2").arg(before->TypeDisplayName(), after->TypeDisplayName());
}

QString LineRouteCommand::LogDetail(bool reverse) const
{
    const int beforeRoute = reverse ? m_afterRoute : m_beforeRoute;
    const int afterRoute = reverse ? m_beforeRoute : m_afterRoute;
    const auto routeName = [](int route) {
        return route == static_cast<int>(StraightLineItem::RouteMode::ORTHOGONAL) ? QStringLiteral("折线")
                                                                                  : QStringLiteral("直连");
    };
    QStringList parts;
    if (beforeRoute != afterRoute) {
        parts.append(QStringLiteral("走线 %1→%2").arg(routeName(beforeRoute), routeName(afterRoute)));
    }
    const QString geometry = DescribeLineGeometryChange(reverse ? m_afterGeometry : m_beforeGeometry,
                                                        reverse ? m_beforeGeometry : m_afterGeometry);
    if (!geometry.isEmpty()) {
        parts.append(geometry);
    }
    return parts.join(QStringLiteral("，"));
}

QString MirrorCommand::LogDetail(bool reverse) const
{
    const QList<MirrorItemState>& before = reverse ? m_after : m_before;
    const QList<MirrorItemState>& after = reverse ? m_before : m_after;
    QStringList parts;
    for (int i = 0; i < before.size() && i < after.size(); ++i) {
        if (!before.at(i).item || before.at(i).item != after.at(i).item) {
            continue;
        }
        if (before.at(i).flipX != after.at(i).flipX) {
            parts.append(QStringLiteral("水平翻转 %1→%2")
                             .arg(before.at(i).flipX ? QStringLiteral("开") : QStringLiteral("关"),
                                  after.at(i).flipX ? QStringLiteral("开") : QStringLiteral("关")));
        }
        if (before.at(i).flipY != after.at(i).flipY) {
            parts.append(QStringLiteral("垂直翻转 %1→%2")
                             .arg(before.at(i).flipY ? QStringLiteral("开") : QStringLiteral("关"),
                                  after.at(i).flipY ? QStringLiteral("开") : QStringLiteral("关")));
        }
        if (before.at(i).pos != after.at(i).pos) {
            parts.append(QStringLiteral("位置 (%1,%2)→(%3,%4)")
                             .arg(LogNumber(before.at(i).pos.x()), LogNumber(before.at(i).pos.y()),
                                  LogNumber(after.at(i).pos.x()), LogNumber(after.at(i).pos.y())));
        }
        if (before.at(i).rotation != after.at(i).rotation) {
            parts.append(
                QStringLiteral("旋转 %1°→%2°").arg(LogNumber(before.at(i).rotation), LogNumber(after.at(i).rotation)));
        }
        if (!parts.isEmpty()) {
            break;
        }
    }
    return parts.join(QStringLiteral("，"));
}

QString StrokeSmoothCommand::LogDetail(bool reverse) const
{
    const QList<LineAbstractItem::GeometryState>& before = reverse ? m_after : m_before;
    const QList<LineAbstractItem::GeometryState>& after = reverse ? m_before : m_after;
    if (before.isEmpty() || after.isEmpty()) {
        return {};
    }
    QString detail = DescribeLineGeometryChange(before.constFirst(), after.constFirst());
    const QList<QPointF>& oldKnots = reverse ? m_afterKnots : m_beforeKnots;
    const QList<QPointF>& newKnots = reverse ? m_beforeKnots : m_afterKnots;
    if (oldKnots.size() == newKnots.size() && oldKnots.size() >= 2) {
        int changed = 0;
        QStringList positions;
        for (int i = 0; i < oldKnots.size(); ++i) {
            if (oldKnots.at(i) == newKnots.at(i)) {
                continue;
            }
            ++changed;
            if (changed > 3) {
                continue;
            }
            const QString label =
                i == 0 ? QStringLiteral("起点")
                       : (i == oldKnots.size() - 1 ? QStringLiteral("终点") : QStringLiteral("拐点 #%1").arg(i + 1));
            positions.append(QStringLiteral("%1 (%2,%3)→(%4,%5)")
                                 .arg(label, LogNumber(oldKnots.at(i).x()), LogNumber(oldKnots.at(i).y()),
                                      LogNumber(newKnots.at(i).x()), LogNumber(newKnots.at(i).y())));
        }
        if (changed > 3) {
            positions.append(QStringLiteral("等 %1 个节点位置变化").arg(changed));
        }
        if (!positions.isEmpty()) {
            if (!detail.isEmpty()) {
                detail += QStringLiteral("，");
            }
            detail += positions.join(QStringLiteral("，"));
        }
    }
    if (before.size() > 1) {
        if (!detail.isEmpty()) {
            detail += QStringLiteral("，");
        }
        detail += QStringLiteral("等 %1 条线").arg(before.size());
    }
    return detail;
}

QString ArrowBindingCommand::LogDetail(bool reverse) const
{
    const QList<ArrowBindingState>& before = reverse ? m_after : m_before;
    const QList<ArrowBindingState>& after = reverse ? m_before : m_after;
    if (before.isEmpty() || after.isEmpty()) {
        return {};
    }
    const auto target = [](const QJsonObject& binding) {
        const QString uid = binding.value(QStringLiteral("elementId")).toString();
        return uid.isEmpty() ? QStringLiteral("无") : uid;
    };
    QStringList parts;
    if (target(before.constFirst().start) != target(after.constFirst().start)) {
        parts.append(
            QStringLiteral("起点绑定 %1→%2").arg(target(before.constFirst().start), target(after.constFirst().start)));
    }
    if (target(before.constFirst().end) != target(after.constFirst().end)) {
        parts.append(
            QStringLiteral("终点绑定 %1→%2").arg(target(before.constFirst().end), target(after.constFirst().end)));
    }
    if (parts.isEmpty()) {
        return {};
    }
    if (before.size() > 1) {
        parts.append(QStringLiteral("等 %1 条线").arg(before.size()));
    }
    return parts.join(QStringLiteral("，"));
}

QString LayerStateCommand::LogDetail(bool reverse) const
{
    return DescribeLayerStackChange(reverse ? m_after : m_before, reverse ? m_before : m_after);
}
