#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "CurveItem.h"
#include "TextItem.h"
#include "scene/SceneConstants.h"

#include <QHash>
#include <QSet>
#include <QUndoStack>
#include <algorithm>
#include <utility>

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
    if (kind == QStringLiteral("curve")) {
        return new CurveItem();
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
    if (kind == QStringLiteral("polygon") || kind == QStringLiteral("composite")) {
        return new CompositeShapeItem();
    }
    return nullptr;
}
