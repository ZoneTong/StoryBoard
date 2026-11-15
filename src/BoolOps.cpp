#include "BoolOps.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "TextItem.h"

#include <QPainterPath>
#include <QSet>
#include <QUndoStack>
#include <algorithm>

namespace {

struct Participant {
    CanvasItem* item = nullptr; // 代表图元：决定样式与落点
    QList<CanvasItem*> members; // 参与替换的全部图元（整组时含组内成员）
    QPainterPath path;          // 场景坐标，整组时为组内矢量轮廓的并集
    QColor fill;
    QColor stroke;
    qreal strokeWidth = 1.0;
    bool filled = true;
    int layerIndex = 0;
    int height = -1;
};

bool priorityLess(CanvasScene* scene, CanvasItem* left, CanvasItem* right)
{
    const int leftLayer = scene->layerIndex(left->layerId());
    const int rightLayer = scene->layerIndex(right->layerId());
    if (leftLayer != rightLayer) {
        return leftLayer < rightLayer;
    }
    if (left->height() != right->height()) {
        return left->height() < right->height();
    }
    return left->zValue() < right->zValue();
}

// 把一批图元合成一个参与者：组按整棵组树算一个轮廓，样式取组内最高优先级
// 的矢量图元；含图片的整组直接跳过，图片不参与合并。
bool appendParticipant(CanvasScene* scene, const QList<CanvasItem*>& members, bool grouped, QList<Participant>* out)
{
    QList<CanvasItem*> editable;
    QList<CanvasItem*> vectorMembers;
    bool hasImage = false;
    for (CanvasItem* member : members) {
        if (!member || !scene->isItemEditable(member)) {
            continue;
        }
        editable.append(member);
        if (dynamic_cast<ImageItem*>(member)) {
            hasImage = true;
        } else {
            vectorMembers.append(member);
        }
    }
    // 图片不参与合并：整组作为一个参与者时，含图片的组直接跳过，避免图片随组被替换。
    if (editable.isEmpty() || vectorMembers.isEmpty() || (grouped && hasImage)) {
        return false;
    }
    std::sort(vectorMembers.begin(), vectorMembers.end(),
              [scene](CanvasItem* left, CanvasItem* right) { return priorityLess(scene, left, right); });

    Participant participant;
    participant.item = vectorMembers.constLast();
    participant.members = editable;
    for (CanvasItem* vectorMember : std::as_const(vectorMembers)) {
        const QPainterPath memberPath = vectorMember->mapToScene(vectorMember->paintedShape());
        if (memberPath.isEmpty()) {
            continue;
        }
        participant.path = participant.path.isEmpty() ? memberPath : participant.path.united(memberPath);
    }
    if (participant.path.isEmpty()) {
        return false;
    }

    CanvasItem* styleItem = participant.item;
    if (auto* shape = dynamic_cast<ShapeItem*>(styleItem)) {
        participant.fill = shape->fillColor();
        participant.stroke = shape->strokeColor();
        participant.strokeWidth = shape->strokeWidth();
        participant.filled = shape->fillStyle() != FillStyle::None;
    } else if (auto* stroke = dynamic_cast<CurveItem*>(styleItem)) {
        participant.fill = stroke->strokeColor();
        participant.stroke = stroke->strokeColor();
        participant.strokeWidth = stroke->strokeWidth();
        participant.filled = false;
    } else if (auto* text = dynamic_cast<TextItem*>(styleItem)) {
        // 文字参照曲线：填充色等于字形描线色、纯色、面积按 0 参与。
        participant.fill = text->textColor();
        participant.stroke = text->textColor();
        participant.strokeWidth = 1.0;
        participant.filled = false;
    } else {
        participant.fill = QColor(30, 41, 59, 36);
        participant.stroke = QColor(30, 41, 59);
    }
    participant.layerIndex = scene->layerIndex(participant.item->layerId());
    participant.height = participant.item->height();
    out->append(participant);
    return true;
}

// 收集参与运算的参与者，并按优先级从低到高排序（图层先比，再比同层 height、
// 最后比 zValue）。整组只算一个参与者，避免组内成员被拆开重复参与。
QList<Participant> collect(CanvasScene* scene)
{
    QList<Participant> participants;
    if (!scene) {
        return participants;
    }
    QSet<int> handledGroups;
    for (CanvasItem* item : scene->selectedCanvasItems()) {
        if (!item || !scene->isItemEditable(item)) {
            continue;
        }
        if (item->groupId() > 0) {
            const int rootGroup = scene->outermostGroup(item->groupId());
            if (handledGroups.contains(rootGroup)) {
                continue;
            }
            handledGroups.insert(rootGroup);
            appendParticipant(scene, scene->itemsInGroupTree(rootGroup), true, &participants);
            continue;
        }
        // 未分组图片没有矢量轮廓，不参与合并图元。
        if (dynamic_cast<ImageItem*>(item)) {
            continue;
        }
        appendParticipant(scene, QList<CanvasItem*>{item}, false, &participants);
    }
    std::sort(participants.begin(), participants.end(), [](const Participant& left, const Participant& right) {
        if (left.layerIndex != right.layerIndex) {
            return left.layerIndex < right.layerIndex;
        }
        if (left.height != right.height) {
            return left.height < right.height;
        }
        return left.item->zValue() < right.item->zValue();
    });
    return participants;
}

// 单层结果统一用最高优先级图元的颜色。
CompositeShapeItem::Layer singleLayer(const QPainterPath& path, const Participant& style)
{
    CompositeShapeItem::Layer layer;
    layer.path = path;
    layer.fill = style.fill;
    layer.stroke = style.stroke;
    layer.strokeWidth = qMax(0.0, style.strokeWidth);
    layer.filled = true;
    return layer;
}

} // namespace

namespace BoolOps {

bool supports(CanvasScene* scene)
{
    return collect(scene).size() >= 2;
}

bool apply(CanvasScene* scene, Operation operation, const QString& commandText)
{
    QList<Participant> participants = collect(scene);
    if (participants.size() < 2) {
        return false;
    }

    // 顶层图元（优先级最高的那个）决定结果的颜色与落点。
    const Participant top = participants.constLast();
    // 除拆分外的运算都只产出一个多边形；拆分产出多个互不重叠的独立图元。
    QList<QList<CompositeShapeItem::Layer>> results;

    switch (operation) {
    case Operation::Union: {
        // 联合：内部共边不描线，整块结果统一采用最高优先级图元的填充与描线。
        QPainterPath result = participants.constFirst().path;
        for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
            result = result.united(participants.at(i).path);
        }
        if (result.isEmpty()) {
            return false;
        }
        results.append(QList<CompositeShapeItem::Layer>{singleLayer(result, top)});
        break;
    }
    case Operation::Combine: {
        // 组合：并集挖掉重叠区，重叠部分变成空洞，整体仍是单层、用顶层颜色。
        QPainterPath result = participants.constFirst().path;
        for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
            const QPainterPath& other = participants.at(i).path;
            result = result.united(other).subtracted(result.intersected(other));
        }
        if (result.isEmpty()) {
            return false;
        }
        results.append(QList<CompositeShapeItem::Layer>{singleLayer(result, top)});
        break;
    }
    case Operation::Fragment: {
        // 拆分：按“被哪些图元覆盖”把重叠区切成互不重叠的多块，每块沿用覆盖它
        // 的最高优先级图元的颜色；每块单独生成一个多边形图元，可分别选中和拖动。
        // 图元数超过 8 个时 2^n 组合过大，取前 8 个参与。
        const int count = qMin(participants.size(), 8);
        for (int mask = 1; mask < (1 << count); ++mask) {
            QPainterPath region;
            bool started = false;
            for (int i = 0; i < count; ++i) {
                if (!(mask & (1 << i))) {
                    continue;
                }
                region = started ? region.intersected(participants.at(i).path) : participants.at(i).path;
                started = true;
            }
            if (region.isEmpty()) {
                continue;
            }
            // 挖掉不在本组合里的图元，得到只在 mask 覆盖下的那一块。
            for (int i = 0; i < participants.size(); ++i) {
                if (i < count && (mask & (1 << i))) {
                    continue;
                }
                region = region.subtracted(participants.at(i).path);
                if (region.isEmpty()) {
                    break;
                }
            }
            if (region.isEmpty()) {
                continue;
            }
            // 拆分出的每一块都沿用同一个 donor（全局最高优先级图元）的样式。
            results.append(QList<CompositeShapeItem::Layer>{singleLayer(region, top)});
        }
        if (results.isEmpty()) {
            return false;
        }
        break;
    }
    case Operation::Intersect: {
        QPainterPath result = participants.constFirst().path;
        for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
            result = result.intersected(participants.at(i).path);
        }
        if (result.isEmpty()) {
            return false;
        }
        // 相交结果同样统一采用最高优先级图元的样式。
        results.append(QList<CompositeShapeItem::Layer>{singleLayer(result, top)});
        break;
    }
    case Operation::Subtract: {
        // 相减：以最上层图元为底，依次挖掉其余图元。
        QPainterPath result = top.path;
        for (int i = 0; i < participants.size() - 1 && !result.isEmpty(); ++i) {
            result = result.subtracted(participants.at(i).path);
        }
        if (result.isEmpty()) {
            return false;
        }
        results.append(QList<CompositeShapeItem::Layer>{singleLayer(result, top)});
        break;
    }
    }

    QList<CanvasItem*> originals;
    QSet<CanvasItem*> originalSet;
    for (const Participant& participant : participants) {
        for (CanvasItem* member : participant.members) {
            if (member && !originalSet.contains(member)) {
                originalSet.insert(member);
                originals.append(member);
            }
        }
    }

    QUndoStack* undo = scene->undoStack();
    if (!undo) {
        return false;
    }

    // 一次操作只占一条撤销记录；拆分产出的多个图元逐条加入同一宏内。
    QList<CanvasItem*> created;
    undo->beginMacro(commandText);
    undo->push(new RemoveItemsCommand(scene, originals, commandText));
    for (const QList<CompositeShapeItem::Layer>& block : results) {
        auto* composite = new CompositeShapeItem(block);
        composite->setName(scene->uniqueItemName(composite->typeDisplayName()));
        composite->setLayerId(top.item->layerId());
        composite->setHeight(scene->nextHeight(top.item->layerId()));
        undo->push(new AddItemCommand(scene, composite, commandText));
        created.append(composite);
    }
    undo->endMacro();
    scene->clearSelection();
    for (CanvasItem* item : created) {
        item->setSelected(true);
    }
    return true;
}

} // namespace BoolOps
