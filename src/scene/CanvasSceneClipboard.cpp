#include "CanvasItem.h"
#include "CanvasScene.h"
#include "ItemGeometry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QSet>

// 剪贴板序列化与方向键微调：复制走图元自身的 toJson/fromJson，粘贴复用副本的
// 命名、层高与撤销入口；微调只改位置，复用几何撤销命令，一次按键一条记录。

QJsonArray CanvasScene::clipboardItems() const
{
    QJsonArray array;
    for (CanvasItem* item : selectedCanvasItems()) {
        if (!item || !isItemEditable(item)) {
            continue;
        }
        QJsonObject json = item->toJson();
        json.insert(QStringLiteral("opacity"), item->itemOpacity());
        array.append(json);
    }
    return array;
}

int CanvasScene::pasteClipboardItems(const QJsonArray& items)
{
    if (items.isEmpty()) {
        return 0;
    }

    QList<CanvasItem*> copies;
    QSet<QString> reservedNames;
    for (const QJsonValue& value : items) {
        const QJsonObject json = value.toObject();
        CanvasItem* copy = createItem(json.value(QStringLiteral("type")).toString());
        if (!copy) {
            continue;
        }
        copy->fromJson(json);
        copy->setItemOpacity(json.value(QStringLiteral("opacity")).toDouble(1.0));
        // 跨文档粘贴时不带原分组关系，避免副本和来源画布的组联动。
        copy->setGroupId(0);
        copy->setPos(copy->pos() + QPointF(20.0, 20.0));

        const QString sourceName = copy->rawName().trimmed();
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

    addItemsWithUndo(copies, tr("Paste items"));
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

int CanvasScene::nudgeSelected(const QPointF& delta)
{
    if (m_mode != ToolMode::Edit || delta.isNull()) {
        return 0;
    }

    QList<CanvasItem*> targets;
    for (CanvasItem* item : selectedCanvasItems()) {
        if (item && isItemEditable(item)) {
            targets.append(item);
        }
    }
    if (targets.isEmpty()) {
        return 0;
    }

    ItemGeometryList before;
    ItemGeometryList after;
    before.reserve(targets.size());
    after.reserve(targets.size());
    for (CanvasItem* item : targets) {
        before.append({item, item->pos(), item->contentSize()});
    }
    for (CanvasItem* item : targets) {
        item->setPos(item->pos() + delta);
    }
    for (CanvasItem* item : targets) {
        after.append({item, item->pos(), item->contentSize()});
    }
    pushGeometry(before, after, tr("Nudge items"));
    return targets.size();
}
