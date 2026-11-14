#include "BoolOps.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "ShapeItem.h"
#include "StrokeItem.h"

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
        } else if (auto* stroke = dynamic_cast<StrokeItem*>(item)) {
            participant.fill = stroke->strokeColor();
            participant.stroke = stroke->strokeColor();
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

// 填充是否可见：填色模式不是“无填充”，且颜色不完全透明。
bool hasVisibleFill(const Participant& participant)
{
    return participant.filled && participant.fill.alpha() > 0;
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
    QList<CompositeShapeItem::Layer> layers;

    switch (operation) {
    case Operation::Union: {
        // 直接按优先级叠加：高层后画，重叠处显示高层的填充与轮廓颜色。
        for (const Participant& participant : participants) {
            CompositeShapeItem::Layer layer;
            layer.path = participant.path;
            layer.fill = participant.fill;
            layer.stroke = participant.stroke;
            layer.strokeWidth = qMax(0.0, participant.strokeWidth);
            layer.filled = participant.filled;
            layers.append(layer);
        }
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
        layers.append(singleLayer(result, top));
        break;
    }
    case Operation::Fragment: {
        // 拆分：按“被哪些图元覆盖”把重叠区切成互不重叠的多块，每块沿用覆盖它
        // 的最高优先级图元的颜色。图元数超过 8 个时 2^n 组合过大，取前 8 个参与。
        const int count = qMin(participants.size(), 8);
        for (int mask = 1; mask < (1 << count); ++mask) {
            QPainterPath region;
            int topOfRegion = -1;
            bool started = false;
            for (int i = 0; i < count; ++i) {
                if (!(mask & (1 << i))) {
                    continue;
                }
                region = started ? region.intersected(participants.at(i).path) : participants.at(i).path;
                started = true;
                topOfRegion = i;
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
            // 取色：本块覆盖到的图元里，从高优先级往下找第一个可见填充色；
            // 全都透明时退回最高优先级图元本身。
            int styleIndex = topOfRegion;
            for (int i = topOfRegion; i >= 0; --i) {
                if (i >= count || !(mask & (1 << i))) {
                    continue;
                }
                if (hasVisibleFill(participants.at(i))) {
                    styleIndex = i;
                    break;
                }
            }
            layers.append(singleLayer(region, participants.at(styleIndex)));
        }
        if (layers.isEmpty()) {
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
        // 相交结果整块都在重叠区，颜色按优先级从高到低取第一个可见填充色。
        int styleIndex = participants.size() - 1;
        for (int i = participants.size() - 1; i >= 0; --i) {
            if (hasVisibleFill(participants.at(i))) {
                styleIndex = i;
                break;
            }
        }
        layers.append(singleLayer(result, participants.at(styleIndex)));
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
        layers.append(singleLayer(result, top));
        break;
    }
    }

    auto* composite = new CompositeShapeItem(layers);
    composite->setName(scene->uniqueItemName(composite->typeDisplayName()));
    composite->setLayerId(top.item->layerId());
    composite->setHeight(scene->nextHeight(top.item->layerId()));

    QList<CanvasItem*> originals;
    for (const Participant& participant : participants) {
        originals.append(participant.item);
    }

    QUndoStack* undo = scene->undoStack();
    if (!undo) {
        return false;
    }
    undo->beginMacro(commandText);
    undo->push(new RemoveItemsCommand(scene, originals, commandText));
    undo->push(new AddItemCommand(scene, composite, commandText));
    undo->endMacro();
    scene->clearSelection();
    composite->setSelected(true);
    return true;
}

} // namespace BoolOps
