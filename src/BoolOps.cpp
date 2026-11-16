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
    QPainterPath fillPath;      // 有面积图形（含多边形）的填充轮廓
    QPainterPath strokePath;    // 零面积描线图元（直线 / 曲线）的描边带
    QPainterPath textPath;      // 文字字形轮廓
    QColor textColor = QColor(30, 41, 59);
    QColor fill;
    QColor stroke;
    qreal strokeWidth = 1.0;
    FillStyle fillStyle = FillStyle::Solid;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    bool filled = true;
    bool fillSpace = false; // 代表图元是否有填充空间（闭合图形 / 含面积层的多边形）
    int layerIndex = 0;
    int height = -1;
};

// 判定图元是否「有填充空间」：闭合图形与含面积层的多边形算有，直线 / 曲线 /
// 文字是零面积描线，算没有。只看几何类型，不看当前填充样式——无填充的闭合
// 图形仍按完整轮廓参与布尔运算，也应优先决定合并结果的样式。
bool hasFillSpace(CanvasItem* item)
{
    if (auto* shape = dynamic_cast<ShapeItem*>(item)) {
        return !shapeTypeIsStrokeOnly(shape->shapeType());
    }
    if (auto* composite = dynamic_cast<CompositeShapeItem*>(item)) {
        for (const CompositeShapeItem::Layer& layer : composite->layers()) {
            if (layer.role == CompositeShapeItem::LayerRole::FilledArea && !layer.path.isEmpty()) {
                return true;
            }
        }
        return false;
    }
    return !dynamic_cast<CurveItem*>(item) && !dynamic_cast<TextItem*>(item);
}

// 合并优先级：有填充空间的图元整体高于零面积描线图元（矩形 > 曲线），同类
// 内部再按图层顺序、同层 height、最后 zValue 比较；排序为升序，末位即 donor。
bool priorityLess(CanvasScene* scene, CanvasItem* left, CanvasItem* right)
{
    const bool leftFilled = hasFillSpace(left);
    const bool rightFilled = hasFillSpace(right);
    if (leftFilled != rightFilled) {
        return !leftFilled;
    }
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
    participant.fillSpace = hasFillSpace(participant.item);
    participant.members = editable;
    for (CanvasItem* vectorMember : std::as_const(vectorMembers)) {
        QPainterPath memberPath;
        QPainterPath fillPart;
        QPainterPath strokePart;
        QPainterPath textPart;

        // 布尔运算使用图元的几何轮廓，而不是“可见涂色区域”：无填充、纹理填充的
        // 闭合图形也必须按完整轮廓参与联合 / 相交 / 相减，否则菱形与五角星这类
        // 图形联合后会只剩描线带，内部被错误地留空。
        if (auto* text = dynamic_cast<TextItem*>(vectorMember)) {
            memberPath = text->mapToScene(text->paintedShape());
            textPart = memberPath;
            participant.textColor = text->textColor();
        } else if (auto* curve = dynamic_cast<CurveItem*>(vectorMember)) {
            Q_UNUSED(curve)
            memberPath = vectorMember->mapToScene(vectorMember->paintedShape());
            strokePart = memberPath;
        } else if (auto* shape = dynamic_cast<ShapeItem*>(vectorMember);
                   shape && shapeTypeIsStrokeOnly(shape->shapeType())) {
            memberPath = shape->mapToScene(shape->paintedShape());
            strokePart = memberPath;
        } else if (auto* shape = dynamic_cast<ShapeItem*>(vectorMember)) {
            memberPath = shape->mapToScene(shape->outlineShape());
            fillPart = memberPath;
        } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(vectorMember)) {
            for (const CompositeShapeItem::Layer& layer : composite->layers()) {
                if (layer.path.isEmpty()) {
                    continue;
                }
                const QPainterPath part = composite->mapToScene(composite->mapLayerPath(layer.path));
                if (layer.role == CompositeShapeItem::LayerRole::FilledArea) {
                    fillPart = fillPart.isEmpty() ? part : fillPart.united(part);
                } else if (layer.role == CompositeShapeItem::LayerRole::StrokeBand) {
                    strokePart = strokePart.isEmpty() ? part : strokePart.united(part);
                } else if (layer.role == CompositeShapeItem::LayerRole::Text) {
                    textPart = textPart.isEmpty() ? part : textPart.united(part);
                    participant.textColor = layer.fill;
                }
            }
            memberPath = fillPart;
            memberPath = memberPath.isEmpty() ? strokePart : memberPath.united(strokePart);
            memberPath = memberPath.isEmpty() ? textPart : memberPath.united(textPart);
        } else {
            memberPath = vectorMember->mapToScene(vectorMember->paintedShape());
            fillPart = memberPath;
        }

        if (memberPath.isEmpty()) {
            continue;
        }
        participant.path = participant.path.isEmpty() ? memberPath : participant.path.united(memberPath);

        // 按绘制方式分组：有面积图形走填充+外轮廓描线；直线 / 曲线走零面积
        // 描线带，不能再叠一层描线，否则线宽会变成“原线宽 + donor 线宽”；
        // 文字只填字形，不描边，避免 donor 描线色在字形外形成重影。
        if (!fillPart.isEmpty()) {
            participant.fillPath = participant.fillPath.isEmpty() ? fillPart : participant.fillPath.united(fillPart);
        }
        if (!strokePart.isEmpty()) {
            participant.strokePath =
                participant.strokePath.isEmpty() ? strokePart : participant.strokePath.united(strokePart);
        }
        if (!textPart.isEmpty()) {
            participant.textPath = participant.textPath.isEmpty() ? textPart : participant.textPath.united(textPart);
        }
    }
    if (participant.path.isEmpty()) {
        return false;
    }

    CanvasItem* styleItem = participant.item;
    if (auto* shape = dynamic_cast<ShapeItem*>(styleItem)) {
        participant.fill = shape->fillColor();
        participant.stroke = shape->strokeColor();
        participant.strokeWidth = shape->strokeWidth();
        participant.fillStyle = shape->fillStyle();
        participant.strokeStyle = shape->strokeStyle();
        participant.filled = shape->fillStyle() != FillStyle::None;
    } else if (auto* composite = dynamic_cast<CompositeShapeItem*>(styleItem)) {
        participant.fill = composite->fillColor();
        participant.stroke = composite->strokeColor();
        participant.strokeWidth = composite->strokeWidth();
        participant.fillStyle = composite->fillStyle();
        participant.strokeStyle = composite->strokeStyle();
        participant.filled = true;
    } else if (auto* stroke = dynamic_cast<CurveItem*>(styleItem)) {
        participant.fill = stroke->strokeColor();
        participant.stroke = stroke->strokeColor();
        participant.strokeWidth = stroke->strokeWidth();
        participant.strokeStyle = stroke->strokeStyle();
        participant.filled = false;
    } else if (auto* text = dynamic_cast<TextItem*>(styleItem)) {
        // 文字参照曲线：填充色等于字形描线色、纯色、面积按 0 参与。
        participant.fill = text->textColor();
        participant.stroke = text->textColor();
        participant.strokeWidth = 1.0;
        participant.fillStyle = FillStyle::Solid;
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

// 收集参与运算的参与者，并按优先级从低到高排序：先比有无填充空间（有填充
// 空间的整体靠后、优先级高），再比图层顺序、同层 height，最后比 zValue。
// 整组只算一个参与者，避免组内成员被拆开重复参与。
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
        if (left.fillSpace != right.fillSpace) {
            return !left.fillSpace;
        }
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

// 按绘制方式分层，避免同一块结果既填充又重复描线：
// 有面积图形一层（填充 + 外轮廓描线），直线 / 曲线一层（已是线宽带，只填充不描线），
// 文字一层（只填字形，不描边）。分层只在结果包含零面积图元或文字时出现。
QList<CompositeShapeItem::Layer> buildLayers(const QPainterPath& result, const QList<Participant>& participants,
                                             const Participant& top)
{
    QPainterPath areas;
    QPainterPath strokes;
    QPainterPath texts;
    QColor textColor = top.stroke;
    int areaRank = -1;
    int strokeRank = -1;
    int textRank = -1;
    for (int i = 0; i < participants.size(); ++i) {
        const Participant& participant = participants.at(i);
        auto append = [&result](QPainterPath& target, const QPainterPath& source) {
            if (source.isEmpty()) {
                return;
            }
            const QPainterPath clipped = source.intersected(result);
            if (clipped.isEmpty()) {
                return;
            }
            target = target.isEmpty() ? clipped : target.united(clipped);
        };
        if (!participant.fillPath.isEmpty()) {
            append(areas, participant.fillPath);
            areaRank = i;
        }
        if (!participant.strokePath.isEmpty()) {
            append(strokes, participant.strokePath);
            strokeRank = i;
        }
        if (!participant.textPath.isEmpty()) {
            append(texts, participant.textPath);
            textRank = i;
            textColor = participant.textColor;
        }
    }

    struct RankedLayer {
        int rank = 0;
        int order = 0;
        CompositeShapeItem::Layer layer;
    };
    QList<RankedLayer> ranked;
    if (!areas.isEmpty()) {
        CompositeShapeItem::Layer layer;
        layer.path = areas;
        layer.fill = top.fill;
        layer.stroke = top.stroke;
        layer.strokeWidth = qMax(0.0, top.strokeWidth);
        layer.filled = true;
        layer.role = CompositeShapeItem::LayerRole::FilledArea;
        ranked.append(RankedLayer{areaRank, 0, layer});
    }
    if (!strokes.isEmpty()) {
        CompositeShapeItem::Layer layer;
        layer.path = strokes;
        layer.fill = top.stroke;
        layer.strokeWidth = 0.0;
        layer.filled = true;
        layer.role = CompositeShapeItem::LayerRole::StrokeBand;
        ranked.append(RankedLayer{strokeRank, 1, layer});
    }
    if (!texts.isEmpty()) {
        CompositeShapeItem::Layer layer;
        layer.path = texts;
        layer.fill = textColor;
        layer.strokeWidth = 0.0;
        layer.filled = true;
        layer.role = CompositeShapeItem::LayerRole::Text;
        ranked.append(RankedLayer{textRank, 2, layer});
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedLayer& left, const RankedLayer& right) {
        if (left.rank != right.rank) {
            return left.rank < right.rank;
        }
        return left.order < right.order;
    });
    QList<CompositeShapeItem::Layer> layers;
    for (const RankedLayer& entry : std::as_const(ranked)) {
        layers.append(entry.layer);
    }
    return layers;
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

    // 优先级最高的图元决定结果的颜色与落点（有填充空间者优先，其次叠放顺序）。
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
        results.append(buildLayers(result, participants, top));
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
        results.append(buildLayers(result, participants, top));
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
            results.append(buildLayers(region, participants, top));
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
        results.append(buildLayers(result, participants, top));
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
        results.append(buildLayers(result, participants, top));
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
        auto* composite = new CompositeShapeItem(block, top.fillStyle, top.strokeStyle);
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
