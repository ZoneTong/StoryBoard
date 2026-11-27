#include "app/LifecycleLog.h"
#include "io/ImageStore.h"
#include "items/CanvasItem.h"
#include "items/ItemSnapshot.h"
#include "items/lines/LineAbstractItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "scene/CanvasScene.h"

#include <QGraphicsView>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <utility>

// 剪贴板序列化与方向键微调：复制走图元自身的 toJson/fromJson，粘贴复用副本的
// 命名、层高与撤销入口；微调只改位置，复用几何撤销命令，一次按键一条记录。

QJsonArray CanvasScene::ClipboardItems() const
{
    QJsonArray array;
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (!item || !IsItemEditable(item)) {
            continue;
        }
        QJsonObject json = item->ToJson();
        json.insert(QStringLiteral("opacity"), item->ItemOpacity());
        array.append(json);
    }
    return array;
}

QJsonObject CanvasScene::ClipboardPayload() const
{
    QJsonObject payload;
    payload.insert(QStringLiteral("format"), QStringLiteral("StoryBoardItems"));
    payload.insert(QStringLiteral("version"), 2);
    const QJsonArray items = ClipboardItems();
    payload.insert(QStringLiteral("items"), items);

    QJsonObject images;
    QSet<QString> usedIds;
    for (CanvasItem* item : SelectedCanvasItems()) {
        auto* shape = dynamic_cast<ShapeAbstractItem*>(item);
        if (!shape || !shape->HasFillImage()) {
            continue;
        }
        const QString id = ImageStore::Store(&images, shape->FillImage());
        if (!id.isEmpty()) {
            usedIds.insert(id);
        }
    }
    if (!usedIds.isEmpty()) {
        payload.insert(QStringLiteral("images"), ImageStore::Compact(images, usedIds));
    }
    return payload;
}

int CanvasScene::PasteClipboardItems(const QJsonArray& items, const QJsonObject& images)
{
    if (items.isEmpty()) {
        return 0;
    }
    // 粘贴期间创建 / 编辑的图元统一标成“粘贴”，便于和鼠标、快捷键操作区分。
    LifecycleLog::EditSourceScope sourceScope(QStringLiteral("paste"));

    QList<CanvasItem*> copies;
    QSet<QString> reservedNames;
    QHash<QString, QString> uidRemap;
    for (const QJsonValue& value : items) {
        const QJsonObject json = value.toObject();
        CanvasItem* copy = CreateItemForJson(json);
        if (!copy) {
            continue;
        }
        copy->FromJson(json);
        // 身份必须换新：原 id 仍指向来源画布里的图元，绑定关系不能跟着复制。
        // 旧 uid 先登记下来，粘贴完成后按这张表把绑定 / 容器引用改到副本上。
        const QString sourceUid = json.value(QStringLiteral("uid")).toString();
        copy->AssignNewUid();
        if (!sourceUid.isEmpty()) {
            uidRemap.insert(sourceUid, copy->Uid());
        }
        copy->SetItemOpacity(json.value(QStringLiteral("opacity")).toDouble(1.0));
        // 信封里带图片表时，按 fillImageId 回填像素；裸数组旧格式没有表就按无图处理。
        if (auto* shape = dynamic_cast<ShapeAbstractItem*>(copy); shape && !shape->FillImageId().isEmpty()) {
            const QPixmap pixmap = ImageStore::PixmapFromEntry(images.value(shape->FillImageId()).toObject());
            if (!pixmap.isNull()) {
                shape->SetFillImage(pixmap, shape->FillImageMode());
                shape->SetFillImageId(json.value(QStringLiteral("fillImageId")).toString());
            }
        }
        // 跨文档粘贴时不带原分组关系，避免副本和来源画布的组联动。
        copy->SetGroupId(0);
        copy->setPos(copy->pos() + QPointF(20.0, 20.0));

        const QString sourceName = copy->CustomName().trimmed();
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

    // 箭头端点绑定与容器文字引用都按 uid 查表：复制侧已经换新 uid，这里必须把
    // 旧 uid 改写成副本的新 uid；既不在副本里、也不在目标画布里的目标视为悬空，
    // 直接清掉引用，避免留下永远匹配不上的关系。
    const auto remapBinding = [this, &uidRemap](const QJsonObject& binding) {
        if (binding.isEmpty()) {
            return binding;
        }
        const QString target = binding.value(QStringLiteral("elementId")).toString();
        if (target.isEmpty()) {
            return binding;
        }
        QJsonObject result = binding;
        const QString mapped = uidRemap.value(target);
        if (!mapped.isEmpty()) {
            result.insert(QStringLiteral("elementId"), mapped);
            return result;
        }
        if (ItemByUid(target)) {
            return result;
        }
        return QJsonObject();
    };
    for (CanvasItem* copy : std::as_const(copies)) {
        if (auto* line = dynamic_cast<LineAbstractItem*>(copy)) {
            line->SetStartBinding(remapBinding(line->StartBinding()));
            line->SetEndBinding(remapBinding(line->EndBinding()));
        } else if (auto* text = dynamic_cast<TextItem*>(copy)) {
            const QString containerUid = text->ContainerUid();
            if (containerUid.isEmpty()) {
                continue;
            }
            const QString mapped = uidRemap.value(containerUid);
            if (!mapped.isEmpty()) {
                text->SetContainerUid(mapped);
            } else if (!ItemByUid(containerUid)) {
                text->SetContainerUid(QString());
            }
        }
    }

    // 需求 2：粘贴落点是视口中心；整块内容超过画布 72% 时先等比缩到 72%，再夹回画布内。
    const QRectF canvas = CanvasRect();
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
            copy->SetContentSize(copy->ContentSize() * scale);
            copy->setPos(block.topLeft() + (anchor - block.topLeft()) * scale);
        }
        block = QRectF(block.topLeft(), block.size() * scale);
    }
    const QPointF delta = target - block.center();
    for (CanvasItem* copy : std::as_const(copies)) {
        copy->setPos(copy->pos() + delta);
    }

    // 粘贴总是落到当前活动图层：副本的 layer 记录的是来源画布的状态，
    // 不代表粘贴意图；同文档跨层复制也不例外。
    const int pasteLayerId = ActiveLayerId();
    for (CanvasItem* copy : std::as_const(copies)) {
        copy->SetLayerId(pasteLayerId);
    }

    AddItemsWithUndo(copies, tr("Paste items"));
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

int CanvasScene::NudgeSelected(const QPointF& delta)
{
    if (m_mode != ToolMode::EDIT || delta.isNull()) {
        return 0;
    }

    // 方向键微调按「最外层组树」展开：选中组内任一成员时，整棵组树一起动，
    // 与拖拽移动组的行为一致；多个选中目标之间按图元指针去重。
    QList<CanvasItem*> targets;
    QSet<CanvasItem*> seen;
    for (CanvasItem* item : SelectedCanvasItems()) {
        if (!item || !IsItemEditable(item)) {
            continue;
        }
        QList<CanvasItem*> members;
        if (item->GroupId() > 0) {
            members = ItemsInGroupTree(OutermostGroup(item->GroupId()));
        } else {
            members.append(item);
        }
        for (CanvasItem* member : members) {
            if (member && IsItemEditable(member) && !seen.contains(member)) {
                seen.insert(member);
                targets.append(member);
            }
        }
    }
    if (targets.isEmpty()) {
        return 0;
    }

    ItemPlacementList before;
    ItemPlacementList after;
    before.reserve(targets.size());
    after.reserve(targets.size());
    for (CanvasItem* item : targets) {
        before.append({item, item->pos(), item->ContentSize()});
    }
    for (CanvasItem* item : targets) {
        item->setPos(item->pos() + delta);
    }
    for (CanvasItem* item : targets) {
        after.append({item, item->pos(), item->ContentSize()});
    }
    // 标记为 nudge 合并：按住方向键连续微调时并入同一条撤销记录。
    PushGeometry(before, after, tr("Nudge items"), QStringLiteral("nudge"));
    return targets.size();
}
