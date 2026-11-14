#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"
#include "scene/SceneConstants.h"

#include <QHash>
#include <QSet>
#include <QUndoStack>
#include <algorithm>
#include <utility>

namespace {

bool nearlyEqual(qreal a, qreal b)
{
    return qAbs(a - b) < 0.01;
}

bool sameGeometry(const ItemGeometry& a, const ItemGeometry& b)
{
    return nearlyEqual(a.pos.x(), b.pos.x()) && nearlyEqual(a.pos.y(), b.pos.y()) &&
           nearlyEqual(a.size.width(), b.size.width()) && nearlyEqual(a.size.height(), b.size.height());
}

} // namespace

QList<CanvasItem*> CanvasScene::canvasItems() const
{
    QList<CanvasItem*> result;
    const QList<QGraphicsItem*> allItems = items(Qt::AscendingOrder);
    result.reserve(allItems.size());
    for (QGraphicsItem* graphicsItem : allItems) {
        if (auto* item = qgraphicsitem_cast<CanvasItem*>(graphicsItem)) {
            result.append(item);
        }
    }
    return result;
}

int CanvasScene::canvasItemCount() const
{
    return canvasItems().size();
}

QList<CanvasItem*> CanvasScene::selectedCanvasItems() const
{
    QList<CanvasItem*> result;
    const QList<QGraphicsItem*> graphicsItems = selectedItems();
    for (QGraphicsItem* graphicsItem : graphicsItems) {
        if (auto* item = qgraphicsitem_cast<CanvasItem*>(graphicsItem)) {
            result.append(item);
        }
    }
    return result;
}

void CanvasScene::refreshViewDependentGeometry()
{
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (item) {
            item->refreshViewDependentGeometry();
        }
    }
}

bool CanvasScene::canSmoothSelectedStrokes() const
{
    if (m_mode != ToolMode::Edit) {
        return false;
    }
    for (CanvasItem* item : selectedCanvasItems()) {
        if (!isItemEditable(item)) {
            continue;
        }
        if (auto* stroke = dynamic_cast<StrokeItem*>(item)) {
            if (!stroke->isEmpty() && stroke->strokePointCount() >= 3) {
                return true;
            }
        }
    }
    return false;
}

int CanvasScene::smoothSelectedStrokes(qreal tolerance)
{
    if (m_mode != ToolMode::Edit) {
        return 0;
    }

    // 参数 0 只关闭“收尾自动平滑”；手动按钮即使在该设置下也执行一次轻量平滑。
    const qreal effectiveTolerance = qMax<qreal>(2.0, tolerance);
    QList<StrokeItem*> items;
    QList<StrokeItem::GeometryState> before;
    QList<StrokeItem::GeometryState> after;
    for (CanvasItem* item : selectedCanvasItems()) {
        if (!isItemEditable(item)) {
            continue;
        }
        auto* stroke = dynamic_cast<StrokeItem*>(item);
        if (!stroke || stroke->isEmpty() || stroke->strokePointCount() < 3) {
            continue;
        }

        const StrokeItem::GeometryState beforeState = stroke->geometryState();
        if (!stroke->smoothStroke(effectiveTolerance)) {
            continue;
        }
        items.append(stroke);
        before.append(beforeState);
        after.append(stroke->geometryState());
    }
    if (items.isEmpty()) {
        return 0;
    }

    if (m_undoStack) {
        m_undoStack->push(new StrokeSmoothCommand(this, items, before, after, tr("Smooth selected strokes")));
    } else {
        for (int i = 0; i < items.size(); ++i) {
            items.at(i)->applyGeometryState(after.at(i));
        }
        updateSceneExtent();
        update();
        emit contentChanged();
    }
    return items.size();
}

QString CanvasScene::uniqueItemName(const QString& base, const CanvasItem* except) const
{
    const QString typeName = base.trimmed();
    const QString prefix = typeName + QLatin1Char(' ');
    int maxSuffix = 0;
    for (CanvasItem* item : canvasItems()) {
        if (!item || item == except) {
            continue;
        }
        const QString existing = item->name();
        if (!existing.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    }
    return QStringLiteral("%1 %2").arg(typeName).arg(maxSuffix + 1);
}

bool CanvasScene::isItemNameAvailable(const QString& name, const CanvasItem* except) const
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty()) {
        return true;
    }
    for (CanvasItem* item : canvasItems()) {
        if (!item || item == except) {
            continue;
        }
        if (item->name().trimmed().compare(candidate, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

QString CanvasScene::uniqueItemNameFor(const QString& requested, const CanvasItem* except) const
{
    static const QSet<QString> reserved;
    return uniqueItemNameFor(requested, except, reserved);
}

QString CanvasScene::uniqueItemNameFor(const QString& requested, const CanvasItem* except,
                                       const QSet<QString>& reserved) const
{
    const QString trimmed = requested.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    // “Hero panel” → base=Hero panel, suffix=0；“矩形 1” → base=矩形, suffix=1。
    QString base = trimmed;
    int suffix = 0;
    const int split = base.lastIndexOf(QLatin1Char(' '));
    if (split > 0) {
        bool ok = false;
        const int value = base.mid(split + 1).toInt(&ok);
        if (ok && value > 0) {
            base = base.left(split).trimmed();
            suffix = value;
        }
    }
    if (base.isEmpty()) {
        base = trimmed;
    }

    const QString prefix = base + QLatin1Char(' ');
    bool exactBase = false;
    int maxSuffix = suffix;

    auto consider = [&](const QString& rawName) {
        const QString existing = rawName.trimmed();
        if (existing.isEmpty()) {
            return;
        }
        if (existing.compare(base, Qt::CaseInsensitive) == 0) {
            exactBase = true;
        }
        if (!existing.startsWith(prefix, Qt::CaseInsensitive)) {
            return;
        }
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    };

    for (CanvasItem* item : canvasItems()) {
        if (!item || item == except) {
            continue;
        }
        consider(item->name());
    }
    for (const QString& name : reserved) {
        consider(name);
    }

    int next = qMax(maxSuffix + 1, exactBase ? 2 : 1);
    if (suffix > 0) {
        next = qMax(next, suffix + 1);
    }
    return prefix + QString::number(next);
}

void CanvasScene::assignAutoItemNames()
{
    for (CanvasItem* item : canvasItems()) {
        if (!item) {
            continue;
        }
        const QString existing = item->rawName().trimmed();
        if (existing.isEmpty()) {
            item->setName(uniqueItemName(item->typeDisplayName(), item));
        } else if (!isItemNameAvailable(existing, item)) {
            item->setName(uniqueItemNameFor(existing, item));
        }
    }
}

void CanvasScene::attachItem(CanvasItem* item)
{
    if (!item) {
        return;
    }

    m_orphans.removeAll(item);
    if (item->scene() != this) {
        addItem(item);
    }

    if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId) {
        item->setLayerId(m_activeLayerId);
    }
    if (item->height() < 0 || item->height() > scene_constants::kMaxItemHeight ||
        heightUsedByOther(item->layerId(), item->height(), item)) {
        const int height = nextHeight(item->layerId());
        item->setHeight(height >= 0 ? height : 0);
    }
    const QString existingName = item->rawName().trimmed();
    if (existingName.isEmpty()) {
        item->setName(uniqueItemName(item->typeDisplayName(), item));
    } else if (!isItemNameAvailable(existingName, item)) {
        item->setName(uniqueItemNameFor(existingName, item));
    }
    applyItemStacking(item);
    applyItemInteraction(item);

    updateSceneExtent();
    update();
    emit contentChanged();
}

void CanvasScene::detachItem(CanvasItem* item)
{
    if (!item) {
        return;
    }

    if (item->scene() == this) {
        removeItem(item);
    }
    if (!m_orphans.contains(item)) {
        m_orphans.append(item);
    }
    item->setSelected(false);

    updateSceneExtent();
    update();
    emit contentChanged();
}

void CanvasScene::deleteSelected()
{
    QList<CanvasItem*> selected;
    const QList<QGraphicsItem*> graphicsSelection = selectedItems();
    for (QGraphicsItem* graphicsItem : graphicsSelection) {
        if (auto* item = qgraphicsitem_cast<CanvasItem*>(graphicsItem)) {
            selected.append(item);
        }
    }
    if (selected.isEmpty()) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new RemoveItemsCommand(this, selected, tr("Delete item")));
    } else {
        for (CanvasItem* item : selected) {
            detachItem(item);
        }
    }
}

int CanvasScene::duplicateSelectedItems()
{
    if (m_mode != ToolMode::Edit) {
        return 0;
    }

    QList<CanvasItem*> sources;
    for (CanvasItem* item : selectedCanvasItems()) {
        if (item && isItemEditable(item)) {
            sources.append(item);
        }
    }
    if (sources.isEmpty()) {
        return 0;
    }

    // 只有在整组成员都被复制时，副本才重新组成一个新组；只复制组内单个图元
    // 时副本保持未分组，避免副本和原组意外联动。
    QHash<int, QList<CanvasItem*>> groupMembers;
    for (CanvasItem* item : canvasItems()) {
        if (item->groupId() > 0) {
            groupMembers[item->groupId()].append(item);
        }
    }
    QHash<int, int> selectedGroupCounts;
    for (CanvasItem* source : sources) {
        if (source->groupId() > 0) {
            ++selectedGroupCounts[source->groupId()];
        }
    }
    QHash<int, int> copyGroupIds;

    QList<CanvasItem*> copies;
    QSet<QString> reservedNames;
    for (CanvasItem* source : sources) {
        CanvasItem* copy = createItem(source->kind());
        if (!copy) {
            continue;
        }
        copy->fromJson(source->toJson());
        copy->setGroupId(0);

        const int sourceGroupId = source->groupId();
        if (sourceGroupId > 0 && selectedGroupCounts.value(sourceGroupId) == groupMembers.value(sourceGroupId).size() &&
            groupMembers.value(sourceGroupId).size() >= 2) {
            if (!copyGroupIds.contains(sourceGroupId)) {
                copyGroupIds.insert(sourceGroupId, m_nextGroupId++);
            }
            copy->setGroupId(copyGroupIds.value(sourceGroupId));
        }

        // 副本与原图元完全重叠会让用户以为没有复制成功，统一向右下错开一点。
        copy->setPos(source->pos() + QPointF(20.0, 20.0));

        const QString sourceName = source->rawName().trimmed();
        if (sourceName.isEmpty()) {
            copy->setName(QString());
        } else {
            const QString copyName = uniqueItemNameFor(sourceName, copy, reservedNames);
            copy->setName(copyName);
            reservedNames.insert(copyName.toCaseFolded());
        }
        copies.append(copy);
    }
    if (copies.isEmpty()) {
        return 0;
    }

    addItemsWithUndo(copies, tr("Duplicate selected items"));
    clearSelection();
    for (CanvasItem* copy : copies) {
        if (isItemEditable(copy)) {
            copy->setSelected(true);
        }
    }
    updateSceneExtent();
    update();
    emit contentChanged();
    return copies.size();
}

void CanvasScene::selectAllItems()
{
    if (m_mode != ToolMode::Edit) {
        return;
    }
    const QList<CanvasItem*> allItems = canvasItems();
    for (CanvasItem* item : allItems) {
        if (isItemEditable(item)) {
            item->setSelected(true);
        }
    }
}

void CanvasScene::pushGeometry(const ItemGeometryList& before, const ItemGeometryList& after, const QString& text)
{
    if (before.size() != after.size()) {
        return;
    }

    ItemGeometryList filteredBefore;
    ItemGeometryList filteredAfter;
    for (int i = 0; i < before.size(); ++i) {
        if (!before.at(i).item || !after.at(i).item) {
            continue;
        }
        if (sameGeometry(before.at(i), after.at(i))) {
            continue;
        }
        filteredBefore.append(before.at(i));
        filteredAfter.append(after.at(i));
    }
    if (filteredBefore.isEmpty()) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new GeometryCommand(this, filteredBefore, filteredAfter, text));
    } else {
        for (const ItemGeometry& geometry : std::as_const(filteredAfter)) {
            if (!geometry.item) {
                continue;
            }
            geometry.item->setPos(geometry.pos);
            geometry.item->setContentSize(geometry.size);
        }
    }
    updateSceneExtent();
}

void CanvasScene::pushRotation(CanvasItem* item, qreal before, qreal after, const QString& text)
{
    if (!item || qAbs(before - after) < 0.0001) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new RotationCommand(this, {item}, {before}, {after}, text));
    } else {
        item->setRotationAngle(after);
    }
    updateSceneExtent();
}

int CanvasScene::rotateSelected(qreal degrees, const QString& commandText)
{
    const QList<CanvasItem*> items = selectedCanvasItems();
    QList<CanvasItem*> changed;
    QList<qreal> before;
    QList<qreal> after;
    for (CanvasItem* item : items) {
        if (!item || !isItemEditable(item)) {
            continue;
        }
        const qreal start = item->rotationAngle();
        const qreal target = CanvasItem::normalizeRotation(start + degrees);
        if (qAbs(start - target) < 0.0001) {
            continue;
        }
        changed.append(item);
        before.append(start);
        after.append(target);
    }
    if (changed.isEmpty()) {
        return 0;
    }

    const QString text = commandText.isEmpty() ? tr("Rotate items") : commandText;
    if (m_undoStack) {
        m_undoStack->push(new RotationCommand(this, changed, before, after, text));
    } else {
        for (int i = 0; i < changed.size(); ++i) {
            changed.at(i)->setRotationAngle(after.at(i));
        }
    }
    updateSceneExtent();
    update();
    return changed.size();
}

void CanvasScene::addItemsWithUndo(const QList<CanvasItem*>& items, const QString& text)
{
    if (items.isEmpty()) {
        return;
    }

    if (!hasLayer(m_activeLayerId)) {
        ensureLayerState();
    }

    // Repair any externally supplied duplicate heights before reserving new
    // slots. Every layer must expose exactly one item per height value.
    normalizeAllHeights();

    // Assign the active layer and a unique height before the items are
    // attached, so a multi-image drop keeps the same top-to-bottom order it
    // had when it was created.
    QHash<int, QSet<int>> usedHeights;
    const QList<CanvasItem*> existing = canvasItems();
    for (const CanvasItem* item : existing) {
        usedHeights[item->layerId()].insert(item->height());
    }

    for (CanvasItem* item : items) {
        if (!item) {
            continue;
        }
        if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId) {
            item->setLayerId(m_activeLayerId);
        }

        QSet<int>& used = usedHeights[item->layerId()];
        int height = item->height();
        if (height < 0 || height > scene_constants::kMaxItemHeight || used.contains(height)) {
            height = nextHeight(item->layerId());
            while (height >= 0 && height < scene_constants::kMaxItemHeight && used.contains(height)) {
                ++height;
            }
            if (height < 0 || used.contains(height)) {
                height = 0;
            }
        }
        item->setHeight(qBound(0, height, scene_constants::kMaxItemHeight));
        used.insert(item->height());
        if (item->groupId() > 0) {
            m_nextGroupId = qMax(m_nextGroupId, item->groupId() + 1);
        }
        applyItemStacking(item);
    }

    if (m_undoStack) {
        if (items.size() == 1) {
            m_undoStack->push(new AddItemCommand(this, items.constFirst(), text));
        } else {
            m_undoStack->beginMacro(text);
            for (CanvasItem* item : items) {
                m_undoStack->push(new AddItemCommand(this, item, text));
            }
            m_undoStack->endMacro();
        }
    } else {
        for (CanvasItem* item : items) {
            attachItem(item);
        }
    }
    updateSceneExtent();
}

void CanvasScene::commitNewItem(CanvasItem* item, const QString& text)
{
    addItemsWithUndo({item}, text);
}

CanvasItem* CanvasScene::createItem(const QString& kind) const
{
    if (kind == QStringLiteral("stroke")) {
        return new StrokeItem();
    }
    if (kind == QStringLiteral("image")) {
        return new ImageItem();
    }
    if (kind == QStringLiteral("text")) {
        return new TextItem();
    }
    if (kind == QStringLiteral("shape")) {
        return new ShapeItem();
    }
    if (kind == QStringLiteral("composite")) {
        return new CompositeShapeItem();
    }
    return nullptr;
}
