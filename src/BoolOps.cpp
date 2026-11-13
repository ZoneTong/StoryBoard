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
    QPainterPath path;   // 场景坐标
    QColor fill;
    QColor stroke;
    qreal strokeWidth = 1.0;
    bool filled = true;
    int layerIndex = 0;
    int height = -1;
};

// 收集参与运算的图元，并按优先级从低到高排序（图层先比，再比同层 height）。
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

    if (operation == Operation::Union) {
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
    } else if (operation == Operation::Intersect) {
        QPainterPath result = participants.constFirst().path;
        for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
            result = result.intersected(participants.at(i).path);
        }
        if (result.isEmpty()) {
            return false;
        }
        CompositeShapeItem::Layer layer;
        layer.path = result;
        layer.fill = top.fill;
        layer.stroke = top.stroke;
        layer.strokeWidth = top.strokeWidth;
        layer.filled = true;
        layers.append(layer);
    } else {
        // 相减：以最上层图元为底，依次挖掉其余图元。
        QPainterPath result = top.path;
        for (int i = 0; i < participants.size() - 1 && !result.isEmpty(); ++i) {
            result = result.subtracted(participants.at(i).path);
        }
        if (result.isEmpty()) {
            return false;
        }
        CompositeShapeItem::Layer layer;
        layer.path = result;
        layer.fill = top.fill;
        layer.stroke = top.stroke;
        layer.strokeWidth = top.strokeWidth;
        layer.filled = true;
        layers.append(layer);
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
