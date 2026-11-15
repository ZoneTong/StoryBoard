#include "BoolOps.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "CurveItem.h"
#include "TextItem.h"

#include <QPainterPath>
#include <QUndoStack>
#include <algorithm>

namespace {

struct Participant {
    CanvasItem* item = nullptr;
    QPainterPath path; // 场景坐标
    QColor fill;
    QColor stroke;
    qreal strokeWidth = 1.0;
    bool filled = true;
    int layerIndex = 0;
    int height = -1;
}; // 收集参与运算的图元，并按优先级从低到高排序（图层先比，再比同层 height）。
QList<Participant> collect(CanvasScene* scene)
{
    QList<Participant> participants;
    if (!scene) {
        return participants;
    }
    for (CanvasItem* item : scene->selectedCanvasItems()) {
        if (!item || !scene->isItemEditable(item)) {
            continue;
        }
        // 图片没有矢量轮廓，不参与合并图元。
        if (dynamic_cast<ImageItem*>(item)) {
            continue;
        }
        Participant participant;
        participant.item = item;
        participant.path = item->mapToScene(item->paintedShape());
        if (participant.path.isEmpty()) {
            continue;
        }
        if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
            participant.fill = shape->fillColor();
            participant.stroke = shape->strokeColor();
            participant.strokeWidth = shape->strokeWidth();
            participant.filled = shape->fillStyle() != FillStyle::None;
        } else if (auto* stroke = dynamic_cast<CurveItem*>(item)) {
            participant.fill = stroke->strokeColor();
            participant.stroke = stroke->strokeColor();
        } else if (auto* text = dynamic_cast<TextItem*>(item)) {
            // 文字参照笔画：填充色等于字形描线色、纯色、面积按 0 参与。
            participant.fill = text->textColor();
            participant.stroke = text->textColor();
            participant.filled = false;
        } else {
            participant.fill = QColor(30, 41, 59, 36);
            participant.stroke = QColor(30, 41, 59);
        }
        participant.layerIndex = scene->layerIndex(item->layerId());
        participant.height = item->height();
        participants.append(participant);
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
    // 除拆分外的运算都只产出一个合并图形；拆分产出多个互不重叠的独立图元。
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
        // 的最高优先级图元的颜色；每块单独生成一个合并图形图元，可分别选中和拖动。
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
    for (const Participant& participant : participants) {
        originals.append(participant.item);
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
