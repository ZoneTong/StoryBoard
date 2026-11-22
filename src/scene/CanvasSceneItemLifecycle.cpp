#include "ArcItem.h"
#include "BraceItem.h"
#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CurveItem.h"
#include "PolygonItem.h"
#include "ShapeAbstractItem.h"
#include "SineItem.h"
#include "StraightLineItem.h"
#include "TextItem.h"
#include "scene/SceneConstants.h"
#include "shapes/ShapeFactory.h"

#include <QHash>
#include <QSet>
#include <QUndoStack>
#include <algorithm>
#include <utility>

void CanvasScene::AttachItem(CanvasItem* item)
{
    if (!item) {
        return;
    }

    m_orphans.removeAll(item);
    if (item->scene() != this) {
        addItem(item);
    }

    if (!HasLayer(item->LayerId()) || item->LayerId() == BackgroundLayerId) {
        item->SetLayerId(m_activeLayerId);
    }
    if (item->ZOrder() < 0 || item->ZOrder() > scene_constants::MAX_ITEM_HEIGHT ||
        HeightUsedByOther(item->LayerId(), item->ZOrder(), item)) {
        const int height = NextHeight(item->LayerId());
        item->SetZOrder(height >= 0 ? height : 0);
    }
    const QString existingName = item->CustomName().trimmed();
    if (existingName.isEmpty()) {
        item->SetName(UniqueItemName(item->TypeDisplayName(), item));
    } else if (!IsItemNameAvailable(existingName, item)) {
        item->SetName(UniqueItemNameFor(existingName, item));
    }
    ApplyItemStacking(item);
    ApplyItemInteraction(item);

    UpdateSceneExtent();
    update();
    emit contentChanged();
}

void CanvasScene::DetachItem(CanvasItem* item)
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

    UpdateSceneExtent();
    update();
    emit contentChanged();
}

void CanvasScene::DeleteSelected()
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
            DetachItem(item);
        }
    }
}

int CanvasScene::DuplicateSelectedItems()
{
    if (m_mode != ToolMode::EDIT) {
        return 0;
    }

    QList<CanvasItem*> sources;
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (item && IsItemEditable(item)) {
            sources.append(item);
        }
    }
    if (sources.isEmpty()) {
        return 0;
    }

    // 只有在整组成员都被复制时，副本才重新组成一个新组；只复制组内单个图元
    // 时副本保持未分组，避免副本和原组意外联动。
    QHash<int, QList<CanvasItem*>> groupMembers;
    for (CanvasItem* item : CanvasItems()) {
        if (item->GroupId() > 0) {
            groupMembers[item->GroupId()].append(item);
        }
    }
    QHash<int, int> selectedGroupCounts;
    for (CanvasItem* source : sources) {
        if (source->GroupId() > 0) {
            ++selectedGroupCounts[source->GroupId()];
        }
    }
    QHash<int, int> copyGroupIds;

    QList<CanvasItem*> copies;
    QSet<QString> reservedNames;
    for (CanvasItem* source : sources) {
        CanvasItem* copy = CreateItemForJson(source->ToJson());
        if (!copy) {
            continue;
        }
        copy->FromJson(source->ToJson());
        copy->SetGroupId(0);

        const int sourceGroupId = source->GroupId();
        if (sourceGroupId > 0 && selectedGroupCounts.value(sourceGroupId) == groupMembers.value(sourceGroupId).size() &&
            groupMembers.value(sourceGroupId).size() >= 2) {
            if (!copyGroupIds.contains(sourceGroupId)) {
                copyGroupIds.insert(sourceGroupId, m_nextGroupId++);
            }
            copy->SetGroupId(copyGroupIds.value(sourceGroupId));
        }

        // 副本与原图元完全重叠会让用户以为没有复制成功，统一向右下错开一点。
        copy->setPos(source->pos() + QPointF(20.0, 20.0));

        const QString sourceName = source->CustomName().trimmed();
        if (sourceName.isEmpty()) {
            copy->SetName(QString());
        } else {
            const QString copyName = UniqueItemNameFor(sourceName, copy, reservedNames);
            copy->SetName(copyName);
            reservedNames.insert(copyName.toCaseFolded());
        }
        copies.append(copy);
    }
    if (copies.isEmpty()) {
        return 0;
    }

    AddItemsWithUndo(copies, tr("Duplicate selected items"));
    clearSelection();
    for (CanvasItem* copy : copies) {
        if (IsItemEditable(copy)) {
            copy->setSelected(true);
        }
    }
    UpdateSceneExtent();
    update();
    emit contentChanged();
    return copies.size();
}

void CanvasScene::AddItemsWithUndo(const QList<CanvasItem*>& items, const QString& text)
{
    if (items.isEmpty()) {
        return;
    }

    if (!HasLayer(m_activeLayerId)) {
        EnsureLayerState();
    }

    // Repair any externally supplied duplicate heights before reserving new
    // slots. Every layer must expose exactly one item per height value.
    NormalizeAllHeights();

    // Assign the active layer and a unique height before the items are
    // attached, so a multi-image drop keeps the same top-to-bottom order it
    // had when it was created.
    QHash<int, QSet<int>> usedHeights;
    const QList<CanvasItem*> existing = CanvasItems();
    for (const CanvasItem* item : existing) {
        usedHeights[item->LayerId()].insert(item->ZOrder());
    }

    for (CanvasItem* item : items) {
        if (!item) {
            continue;
        }
        if (!HasLayer(item->LayerId()) || item->LayerId() == BackgroundLayerId) {
            item->SetLayerId(m_activeLayerId);
        }

        QSet<int>& used = usedHeights[item->LayerId()];
        int height = item->ZOrder();
        if (height < 0 || height > scene_constants::MAX_ITEM_HEIGHT || used.contains(height)) {
            height = NextHeight(item->LayerId());
            while (height >= 0 && height < scene_constants::MAX_ITEM_HEIGHT && used.contains(height)) {
                ++height;
            }
            if (height < 0 || used.contains(height)) {
                height = 0;
            }
        }
        item->SetZOrder(qBound(0, height, scene_constants::MAX_ITEM_HEIGHT));
        used.insert(item->ZOrder());
        if (item->GroupId() > 0) {
            m_nextGroupId = qMax(m_nextGroupId, item->GroupId() + 1);
        }
        ApplyItemStacking(item);
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
            AttachItem(item);
        }
    }
    UpdateSceneExtent();
}

void CanvasScene::CommitNewItem(CanvasItem* item, const QString& text)
{
    AddItemsWithUndo({item}, text);
}

CanvasItem* CanvasScene::CreateItem(const QString& kind, const QString& shapeType) const
{
    // 类型标识只按 kind 建类：线条族五个形态各自一个类，图形 / 多边形 / 文字各一个族。
    if (kind == QStringLiteral("straightline")) {
        return new StraightLineItem();
    }
    if (kind == QStringLiteral("arc")) {
        return new ArcItem();
    }
    if (kind == QStringLiteral("sine")) {
        return new SineItem();
    }
    if (kind == QStringLiteral("freehand")) {
        return new CurveItem();
    }
    if (kind == QStringLiteral("brace")) {
        return new BraceItem();
    }
    if (kind == QStringLiteral("text")) {
        return new TextItem();
    }
    if (kind == QStringLiteral("shape")) {
        return CreateShapeItem(ShapeTypeFromString(shapeType));
    }
    if (kind == QStringLiteral("polygon")) {
        return new PolygonItem();
    }
    return nullptr;
}

CanvasItem* CanvasScene::CreateItemForJson(const QJsonObject& json) const
{
    // 旧画布把类型写在 type 里，kind 是 0.1.4 起的键；曲线族改名前的
    // curve / line 也在这里折到新 kind，保证示例画布与自定义模板能直接打开。
    QString kind = json.value(QStringLiteral("kind")).toString();
    if (kind.isEmpty()) {
        kind = json.value(QStringLiteral("type")).toString();
    }
    if (kind == QStringLiteral("curve")) {
        kind = QStringLiteral("freehand");
    } else if (kind == QStringLiteral("line")) {
        kind = QStringLiteral("straightline");
    }
    return CreateItem(kind, json.value(QStringLiteral("shapeType")).toString());
}
