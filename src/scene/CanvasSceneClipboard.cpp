#include "CanvasItem.h"
#include "CanvasScene.h"
#include "ItemGeometry.h"

#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <utility>

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

    // 需求 2：粘贴落点是视口中心；整块内容超过画布 72% 时先等比缩到 72%，再夹回画布内。
    const QRectF canvas = canvasRect();
    QRectF block;
    for (CanvasItem* copy : std::as_const(copies)) {
        const QRectF itemRect = copy->sceneBoundingRect();
        block = block.isNull() ? itemRect : block.united(itemRect);
    }
    const qreal maxWidth = canvas.width() * 0.72;
    const qreal maxHeight = canvas.height() * 0.72;
    qreal scale = 1.0;
    if (block.width() > 0.0 && block.height() > 0.0) {
        scale = qMin<qreal>(1.0, qMin(maxWidth / block.width(), maxHeight / block.height()));
    }
    QPointF target = canvas.center();
    const QList<QGraphicsView*> views = this->views();
    if (!views.isEmpty() && views.constFirst() && views.constFirst()->viewport()) {
        target = views.constFirst()->mapToScene(views.constFirst()->viewport()->rect().center());
    }
    if (!canvas.contains(target)) {
        target = canvas.center();
    }
    if (!qFuzzyCompare(scale, 1.0) && !block.isEmpty()) {
        for (CanvasItem* copy : std::as_const(copies)) {
            const QPointF anchor = copy->pos();
            copy->setContentSize(copy->contentSize() * scale);
            copy->setPos(block.topLeft() + (anchor - block.topLeft()) * scale);
        }
        block = QRectF(block.topLeft(), block.size() * scale);
    }
    const QPointF delta = target - block.center();
    for (CanvasItem* copy : std::as_const(copies)) {
        copy->setPos(copy->pos() + delta);
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

    // 方向键微调按「最外层组树」展开：选中组内任一成员时，整棵组树一起动，
    // 与拖拽移动组的行为一致；多个选中目标之间按图元指针去重。
    QList<CanvasItem*> targets;
    QSet<CanvasItem*> seen;
    for (CanvasItem* item : selectedCanvasItems()) {
        if (!item || !isItemEditable(item)) {
            continue;
        }
        QList<CanvasItem*> members;
        if (item->groupId() > 0) {
            members = itemsInGroupTree(outermostGroup(item->groupId()));
        } else {
            members.append(item);
        }
        for (CanvasItem* member : members) {
            if (member && isItemEditable(member) && !seen.contains(member)) {
                seen.insert(member);
                targets.append(member);
            }
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
    // 标记为 nudge 合并：按住方向键连续微调时并入同一条撤销记录。
    pushGeometry(before, after, tr("Nudge items"), QStringLiteral("nudge"));
    return targets.size();
}
