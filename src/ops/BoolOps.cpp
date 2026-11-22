#include "ops/BoolOps.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonAccess.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "items/text/TextItem.h"
#include "ops/LineSplitOps.h"
#include "ops/MergeRegions.h"
#include "ops/PathRegions.h"
#include "scene/CanvasScene.h"
#include "scene/Commands.h"

#include <QPainterPath>
#include <QPainterPathStroker>
#include <QSet>
#include <QUndoStack>
#include <algorithm>
#include <climits>

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
    FillStyle fillStyle = FillStyle::SOLID;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    bool filled = true;
    bool fillSpace = false;       // 代表图元是否有填充空间（闭合图形 / 含面积层的多边形）
    qreal originalFillArea = 0.0; // 合并前自身填充面积（D2 豁免判定用）
    int layerIndex = 0;
    int height = -1;
    CanvasItem* stackItem = nullptr; // 叠放位置最高（后画）的成员，用于保持遮挡关系
    int stackRank = 0;               // 合并前叠放序号（0 最先画），决定结果各层绘制顺序
    int selectionIndex = INT_MAX;    // 选择顺序（小者先选中），剪除时用来挑底图
};

// 线条类图元不参与合并：直线（含直线箭头）、括号、弧、曲线，以及纯描线多边形。
bool IsLineClassItem(CanvasItem* item)
{
    if (dynamic_cast<LineAbstractItem*>(item)) {
        return true;
    }
    // 零面积图元已全部并入曲线族，普通形状类恒有填充面积；多边形要看图层。
    if (PolygonItem* polygon = AsPolygon(item)) {
        return !polygon->HasFillAreaLayer();
    }
    return false;
}

// 判定图元是否「有填充空间」：闭合图形与含面积层的多边形算有，直线 / 曲线 /
// 文字是零面积描线，算没有。只看几何类型，不看当前填充样式——无填充的闭合
// 图形仍按完整轮廓参与布尔运算，也应优先决定合并结果的样式。
bool HasFillSpace(CanvasItem* item)
{
    if (PolygonItem* polygon = AsPolygon(item)) {
        return polygon->HasFillAreaLayer();
    }
    return !dynamic_cast<LineAbstractItem*>(item) && item->Category() != ItemCategory::TEXT;
}

// D2：原输入本身 < 阈值的图元豁免碎块规则；块中心落在某个豁免参与者填充区内即豁免，
// 不进入先吸收后删除流程，避免误删用户有意画的小图元。
bool IsBlockExempt(const QPainterPath& block, const QList<Participant>& participants)
{
    const QRectF bounds = block.boundingRect();
    if (bounds.isEmpty()) {
        return false;
    }
    const QPointF center = bounds.center();
    for (const Participant& participant : participants) {
        if (participant.originalFillArea < MergeRegions::MIN_FRAGMENT_AREA && !participant.fillPath.isEmpty() &&
            participant.fillPath.contains(center)) {
            return true;
        }
    }
    return false;
}

// 合并前的叠放顺序（决定合并后各层的绘制顺序，保持遮挡关系）：
// 图层在下 → 同层 height 小在下 → zValue 小在下。
bool StackLess(CanvasScene* scene, CanvasItem* left, CanvasItem* right)
{
    const int leftLayer = scene->LayerIndex(left->LayerId());
    const int rightLayer = scene->LayerIndex(right->LayerId());
    if (leftLayer != rightLayer) {
        return leftLayer < rightLayer;
    }
    if (left->ZOrder() != right->ZOrder()) {
        return left->ZOrder() < right->ZOrder();
    }
    return left->zValue() < right->zValue();
}

// 合并优先级（只决定样式 donor，不决定绘制顺序）：有填充空间的图元整体高于
// 零面积描线图元（矩形 > 曲线），同类内部再按叠放顺序比较；排序为升序，末位
// 即 donor。绘制顺序由 stackRank 另行保证。
bool PriorityLess(CanvasScene* scene, CanvasItem* left, CanvasItem* right)
{
    const bool leftFilled = HasFillSpace(left);
    const bool rightFilled = HasFillSpace(right);
    if (leftFilled != rightFilled) {
        return !leftFilled;
    }
    return StackLess(scene, left, right);
}

// 把一批图元合成一个参与者：组按整棵组树算一个轮廓，样式取组内最高优先级的矢量图元。

bool AppendParticipant(CanvasScene* scene, const QList<CanvasItem*>& members, bool grouped, int selectionIndex,
                       QList<Participant>* out)
{
    Q_UNUSED(grouped);
    QList<CanvasItem*> editable;
    QList<CanvasItem*> vectorMembers;
    for (CanvasItem* member : members) {
        if (!member || !scene->IsItemEditable(member)) {
            continue;
        }
        editable.append(member);
        vectorMembers.append(member);
    }
    // 需求 14：图片类型图元已拆除（图片只作矩形背景），参与者只剩矢量图元。
    if (editable.isEmpty() || vectorMembers.isEmpty()) {
        return false;
    }
    std::sort(vectorMembers.begin(), vectorMembers.end(),
              [scene](CanvasItem* left, CanvasItem* right) { return PriorityLess(scene, left, right); });

    Participant participant;
    participant.item = vectorMembers.constLast();
    participant.fillSpace = HasFillSpace(participant.item);
    // 叠放最高的成员代表该参与者在合并前的可见位置；合并后它所在的层保持后画。
    QList<CanvasItem*> byStacking = vectorMembers;
    std::sort(byStacking.begin(), byStacking.end(),
              [scene](CanvasItem* left, CanvasItem* right) { return StackLess(scene, left, right); });
    participant.stackItem = byStacking.constLast();
    participant.members = editable;
    for (CanvasItem* vectorMember : std::as_const(vectorMembers)) {
        QPainterPath memberPath;
        QPainterPath fillPart;
        QPainterPath strokePart;
        QPainterPath textPart;

        // 布尔运算使用图元的几何轮廓，而不是“可见涂色区域”：无填充、纹理填充的
        // 闭合图形也必须按完整轮廓参与联合 / 相交 / 相减，否则菱形与五角星这类
        // 图形联合后会只剩描线带，内部被错误地留空。
        if (vectorMember->Category() == ItemCategory::TEXT) {
            memberPath = vectorMember->mapToScene(vectorMember->PaintedShape());
            textPart = memberPath;
            participant.textColor = vectorMember->PropertyValue(ItemProperty::TEXT_COLOR).value<QColor>();
        } else if (auto* curve = dynamic_cast<LineAbstractItem*>(vectorMember)) {
            Q_UNUSED(curve)
            memberPath = vectorMember->mapToScene(vectorMember->PaintedShape());
            strokePart = memberPath;
        } else if (PolygonItem* polygon = AsPolygon(vectorMember)) {
            for (const PolygonItem::Layer& layer : polygon->Layers()) {
                if (layer.path.isEmpty()) {
                    continue;
                }
                const QPainterPath part = vectorMember->mapToScene(polygon->MapLayerPath(layer.path));
                if (layer.role == PolygonItem::LayerRole::FILLED_AREA) {
                    fillPart = fillPart.isEmpty() ? part : fillPart.united(part);
                } else if (layer.role == PolygonItem::LayerRole::STROKE_BAND) {
                    strokePart = strokePart.isEmpty() ? part : strokePart.united(part);
                } else if (layer.role == PolygonItem::LayerRole::TEXT) {
                    textPart = textPart.isEmpty() ? part : textPart.united(part);
                    participant.textColor = layer.fill;
                }
            }
            memberPath = fillPart;
            memberPath = memberPath.isEmpty() ? strokePart : memberPath.united(strokePart);
            memberPath = memberPath.isEmpty() ? textPart : memberPath.united(textPart);
        } else if (auto* shape = dynamic_cast<ShapeAbstractItem*>(vectorMember)) {
            memberPath = shape->mapToScene(shape->OutlineShape());
            fillPart = memberPath;
        } else {
            memberPath = vectorMember->mapToScene(vectorMember->PaintedShape());
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
    participant.originalFillArea = PathRegions::Area(participant.fillPath);

    CanvasItem* styleItem = participant.item;
    if (auto* shape = dynamic_cast<ShapeAbstractItem*>(styleItem)) {
        // 多边形与普通图形共用这一段样式提取：填充 / 描线已按各自实现统一收口；
        // 多边形的层区域始终按填充参与，普通图形看填充样式是否为 NONE。
        participant.fill = shape->FillColor();
        participant.stroke = shape->StrokeColor();
        participant.strokeWidth = shape->StrokeWidth();
        participant.fillStyle = shape->fillStyle();
        participant.strokeStyle = shape->StrokeStyle();
        participant.filled = AsPolygon(styleItem) || shape->fillStyle() != FillStyle::NONE;
    } else if (auto* stroke = dynamic_cast<LineAbstractItem*>(styleItem)) {
        participant.fill = stroke->StrokeColor();
        participant.stroke = stroke->StrokeColor();
        participant.strokeWidth = stroke->StrokeWidth();
        participant.strokeStyle = stroke->strokePattern();
        participant.filled = false;
    } else if (styleItem->Category() == ItemCategory::TEXT) {
        // 文字参照曲线：填充色等于字形描线色、纯色、面积按 0 参与。
        const QColor textColor = styleItem->PropertyValue(ItemProperty::TEXT_COLOR).value<QColor>();
        participant.fill = textColor;
        participant.stroke = textColor;
        participant.strokeWidth = 1.0;
        participant.fillStyle = FillStyle::SOLID;
        participant.filled = false;
    } else {
        participant.fill = QColor(30, 41, 59, 36);
        participant.stroke = QColor(30, 41, 59);
    }
    participant.layerIndex = scene->LayerIndex(participant.item->LayerId());
    participant.height = participant.item->ZOrder();
    participant.selectionIndex = selectionIndex;
    out->append(participant);
    return true;
}

// 收集参与运算的参与者：先记录合并前叠放序号（绘制顺序用），再按样式优先级
// 从低到高排序（有填充空间整体靠后、优先级高，末位即 donor）。整组只算一个
// 参与者，避免组内成员被拆开重复参与。
QList<Participant> Collect(CanvasScene* scene)
{
    QList<Participant> participants;
    if (!scene) {
        return participants;
    }
    // 选择顺序：先选中的在前，框选时最底层在前（剪除用它挑底图）。
    QList<CanvasItem*> ordered = scene->SelectionOrder();
    QHash<CanvasItem*, int> selectionIndex;
    for (int i = 0; i < ordered.size(); ++i) {
        selectionIndex.insert(ordered.at(i), i);
    }
    QSet<int> handledGroups;
    for (CanvasItem* item : ordered) {
        if (!item || !scene->IsItemEditable(item)) {
            continue;
        }
        const int order = selectionIndex.value(item, INT_MAX);
        if (item->GroupId() > 0) {
            const int rootGroup = scene->OutermostGroup(item->GroupId());
            if (handledGroups.contains(rootGroup)) {
                continue;
            }
            handledGroups.insert(rootGroup);
            const QList<CanvasItem*> members = scene->ItemsInGroupTree(rootGroup);
            // 组内含线条类图元时整组跳过：线条不参与合并，也不能随组一起被替换掉。
            bool hasLine = false;
            for (CanvasItem* member : members) {
                if (member && IsLineClassItem(member)) {
                    hasLine = true;
                    break;
                }
            }
            if (hasLine) {
                continue;
            }
            AppendParticipant(scene, members, true, order, &participants);
            continue;
        }
        // 线条类图元不参与合并，留在画布上不动。
        if (IsLineClassItem(item)) {
            continue;
        }
        AppendParticipant(scene, QList<CanvasItem*>{item}, false, order, &participants);
    }
    // 先按合并前叠放顺序记序号：合并结果各层按它排绘制顺序，保证合并前
    // 可见（在上层 / 更高 height）的部分合并后仍后画、仍可见。
    QList<int> paintOrder;
    paintOrder.reserve(participants.size());
    for (int i = 0; i < participants.size(); ++i) {
        paintOrder.append(i);
    }
    std::sort(paintOrder.begin(), paintOrder.end(), [scene, &participants](int left, int right) {
        return StackLess(scene, participants.at(left).stackItem, participants.at(right).stackItem);
    });
    for (int rank = 0; rank < paintOrder.size(); ++rank) {
        participants[paintOrder.at(rank)].stackRank = rank;
    }

    // 优先级排序只决定 donor：先比有无填充空间，同类内按叠放序号（stackRank
    // 已随参与者记录，排序后仍然有效）。
    std::sort(participants.begin(), participants.end(), [](const Participant& left, const Participant& right) {
        if (left.fillSpace != right.fillSpace) {
            return !left.fillSpace;
        }
        return left.stackRank < right.stackRank;
    });
    return participants;
}

// 按绘制方式分层，避免同一块结果既填充又重复描线：
// 有面积图形一层（填充 + 外轮廓描线），直线 / 曲线一层（已是线宽带，只填充不描线），
// 文字一层（只填字形，不描边）。分层只在结果包含零面积图元或文字时出现。
// intersectMode：相交结果的填充轮廓先挖掉线宽带（条带区域直接归线宽带层渲染），
// 避免细窄交集条带被当成面积层套上描线后变成黑块。
QList<PolygonItem::Layer> BuildLayers(const QPainterPath& result, const QList<Participant>& participants,
                                      const Participant& top, bool intersectMode)
{
    QPainterPath bandUnion;
    for (const Participant& participant : participants) {
        if (!participant.strokePath.isEmpty()) {
            bandUnion = bandUnion.isEmpty() ? participant.strokePath : bandUnion.united(participant.strokePath);
        }
    }
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
        // 相交模式：填充轮廓先挖掉线宽带，交集条带整体交给线宽带层渲染；
        // 线宽带与文字来源本身就要保留，不做挖除。
        auto appendFill = [&append, intersectMode, &bandUnion](QPainterPath& target, const QPainterPath& source) {
            append(target, intersectMode ? source.subtracted(bandUnion) : source);
        };
        // 每个角色层的绘制序号取该角色贡献者中叠放序号最大者（最靠上的），
        // 保持合并前的遮挡关系；participants 已按优先级排序，不能用下标 i。
        if (!participant.fillPath.isEmpty()) {
            appendFill(areas, participant.fillPath);
            areaRank = areaRank < 0 ? participant.stackRank : qMax(areaRank, participant.stackRank);
        }
        if (!participant.strokePath.isEmpty()) {
            append(strokes, participant.strokePath);
            strokeRank = strokeRank < 0 ? participant.stackRank : qMax(strokeRank, participant.stackRank);
        }
        if (!participant.textPath.isEmpty()) {
            append(texts, participant.textPath);
            textRank = textRank < 0 ? participant.stackRank : qMax(textRank, participant.stackRank);
            textColor = participant.textColor;
        }
    }

    struct RankedLayer {
        int rank = 0;
        int order = 0;
        PolygonItem::Layer layer;
    };
    QList<RankedLayer> ranked;
    if (!areas.isEmpty()) {
        PolygonItem::Layer layer;
        layer.path = areas;
        layer.fill = top.fill;
        layer.stroke = top.stroke;
        layer.strokeWidth = qMax(0.0, top.strokeWidth);
        layer.filled = true;
        layer.role = PolygonItem::LayerRole::FILLED_AREA;
        // 区域整体落在自身 3×描线宽的描边带内（最大内切半径不足 1.5 倍线宽，
        // 描边后剩余可见填充不足半条线宽）时降级为线宽带：只填充不描边，
        // 避免细窄交集 / 窄条被描线吞成黑块；描线宽为 0 时无需降级。
        if (layer.strokeWidth > 0.0) {
            QPainterPathStroker stroker;
            stroker.setJoinStyle(Qt::MiterJoin);
            stroker.setWidth(layer.strokeWidth * 3.0);
            if (areas.subtracted(stroker.createStroke(areas)).isEmpty()) {
                layer.strokeWidth = 0.0;
                layer.role = PolygonItem::LayerRole::STROKE_BAND;
            }
        }
        ranked.append(RankedLayer{areaRank, 0, layer});
    }
    if (!strokes.isEmpty()) {
        PolygonItem::Layer layer;
        layer.path = strokes;
        layer.fill = top.stroke;
        layer.strokeWidth = 0.0;
        layer.filled = true;
        layer.role = PolygonItem::LayerRole::STROKE_BAND;
        ranked.append(RankedLayer{strokeRank, 1, layer});
    }
    if (!texts.isEmpty()) {
        PolygonItem::Layer layer;
        layer.path = texts;
        layer.fill = textColor;
        layer.strokeWidth = 0.0;
        layer.filled = true;
        layer.role = PolygonItem::LayerRole::TEXT;
        ranked.append(RankedLayer{textRank, 2, layer});
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedLayer& left, const RankedLayer& right) {
        if (left.rank != right.rank) {
            return left.rank < right.rank;
        }
        return left.order < right.order;
    });
    QList<PolygonItem::Layer> layers;
    for (const RankedLayer& entry : std::as_const(ranked)) {
        layers.append(entry.layer);
    }
    return layers;
}

} // namespace

namespace BoolOps {

bool Supports(CanvasScene* scene)
{
    return Collect(scene).size() >= 2;
}

bool Apply(CanvasScene* scene, Operation operation, const QString& commandText)
{
    if (operation == Operation::LINE_SPLIT) {
        // 需求 12 方案 2：线条分割有自己的参与者规则（≥1 填充图元 + 恰 1 条切刀），
        // 交给独立实现，避免共用 collect 的「线条不参与」过滤。
        return LineSplitOps::Apply(scene, commandText);
    }
    QList<Participant> participants = Collect(scene);
    if (participants.size() < 2) {
        return false;
    }

    // 优先级最高的图元决定结果的颜色与落点（有填充空间者优先，其次叠放顺序）。
    const Participant top = participants.constLast();
    const int topIndex = participants.size() - 1;

    // 零面积描线（直线 / 曲线 / 文字）只做覆盖着色，不作为挖除与分块的边界：
    // 否则菱形挖直线会在图形上留细缝、交集条带被当成面积层套上描线后变成黑块。
    // 没有任何填充参与者时退回按线宽带运算的旧行为。
    QPainterPath fillUnion;
    QPainterPath bandUnion;
    QPainterPath textUnion;
    bool hasFillParticipant = false;
    for (const Participant& participant : participants) {
        if (!participant.fillPath.isEmpty()) {
            fillUnion = fillUnion.isEmpty() ? participant.fillPath : fillUnion.united(participant.fillPath);
            hasFillParticipant = true;
        }
        if (!participant.strokePath.isEmpty()) {
            bandUnion = bandUnion.isEmpty() ? participant.strokePath : bandUnion.united(participant.strokePath);
        }
        if (!participant.textPath.isEmpty()) {
            textUnion = textUnion.isEmpty() ? participant.textPath : textUnion.united(participant.textPath);
        }
    }

    // 剪除底图：最先选中（框选最底层）为底；其余有填充空间的图元挖除它。
    int baseIndex = 0;
    for (int i = 1; i < participants.size(); ++i) {
        const int candidate = participants.at(i).selectionIndex;
        const int current = participants.at(baseIndex).selectionIndex;
        if (candidate == INT_MAX && current == INT_MAX) {
            // 没有选择顺序信息时退回最底层（stackRank 小者先画、位于下方）。
            if (participants.at(i).stackRank < participants.at(baseIndex).stackRank) {
                baseIndex = i;
            }
        } else if (candidate < current) {
            baseIndex = i;
        }
    }

    // 各运算先算出原始结果区域（场景坐标），再统一走「拆分连续块 → 极小碎块先吸收后
    // 删除 → 允许掩膜校验」的不变量管线（§5.2、§5.3）。
    QList<QPair<QPainterPath, int>> rawRegions;
    auto appendRegions = [&](const QPainterPath& result, int sourceIndex) {
        if (result.isEmpty()) {
            return;
        }
        for (const QPainterPath& piece : PathRegions::SplitDisjoint(result)) {
            rawRegions.append({piece, sourceIndex});
        }
    };

    switch (operation) {
    case Operation::UNION: {
        // 联合：内部共边不描线，整块结果统一采用最高优先级图元的填充与描线。
        QPainterPath result = participants.constFirst().path;
        for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
            result = result.united(participants.at(i).path);
        }
        appendRegions(result, topIndex);
        break;
    }
    case Operation::XOR: {
        if (!hasFillParticipant) {
            // 异或（XOR）：并集减重叠，重叠部分变成空洞，整体仍是单层、用顶层颜色。
            QPainterPath result = participants.constFirst().path;
            for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
                const QPainterPath& other = participants.at(i).path;
                result = result.united(other).subtracted(result.intersected(other));
            }
            appendRegions(result, topIndex);
            break;
        }
        // 只在有填充空间的图元之间挖重叠区；线宽带与文字不参与挖除，
        // 未被填充覆盖的部分作为覆盖层保留。
        QPainterPath punched;
        bool started = false;
        for (const Participant& participant : participants) {
            if (participant.fillPath.isEmpty()) {
                continue;
            }
            punched = started
                          ? punched.united(participant.fillPath).subtracted(punched.intersected(participant.fillPath))
                          : participant.fillPath;
            started = true;
        }
        const QPainterPath result = punched.united(bandUnion.subtracted(fillUnion)).united(textUnion);
        appendRegions(result, topIndex);
        break;
    }
    case Operation::FRAGMENT: {
        // 拆分：只在填充图形之间按「被哪些图形覆盖」把重叠区切成互不重叠的多块，
        // 每块单独生成一个多边形图元，可分别选中和拖动；文字与线条类不参与拆分，
        // 也不单独成块。参与者超过 3 个时组合数超过 8，只取优先级最低的 3 个。
        QList<const Participant*> fillers;
        for (const Participant& participant : participants) {
            if (!participant.fillPath.isEmpty()) {
                fillers.append(&participant);
            }
        }
        if (fillers.size() < 2) {
            return false;
        }
        const int count = qMin(fillers.size(), 3);
        int fragmentSource = 1000;
        for (int mask = 1; mask < (1 << count) && rawRegions.size() < 8; ++mask) {
            QPainterPath region;
            bool started = false;
            for (int i = 0; i < count; ++i) {
                if (!(mask & (1 << i))) {
                    continue;
                }
                region = started ? region.intersected(fillers.at(i)->fillPath) : fillers.at(i)->fillPath;
                started = true;
            }
            if (region.isEmpty()) {
                continue;
            }
            for (int i = 0; i < count; ++i) {
                if ((mask & (1 << i))) {
                    continue;
                }
                region = region.subtracted(fillers.at(i)->fillPath);
                if (region.isEmpty()) {
                    break;
                }
            }
            if (region.isEmpty()) {
                continue;
            }
            // 每个组合区域是独立来源结果：碎块不跨组合互相吸收。
            appendRegions(region, fragmentSource++);
        }
        break;
    }
    case Operation::INTERSECT: {
        QPainterPath result = participants.constFirst().path;
        for (int i = 1; i < participants.size() && !result.isEmpty(); ++i) {
            result = result.intersected(participants.at(i).path);
        }
        appendRegions(result, topIndex);
        break;
    }
    case Operation::SUBTRACT: {
        const Participant& base = participants.at(baseIndex);
        // 底图只被其余有填充空间的图元挖除；文字不参与裁剪。
        QPainterPath otherFills;
        for (int i = 0; i < participants.size(); ++i) {
            if (i == baseIndex) {
                continue;
            }
            const Participant& participant = participants.at(i);
            if (!participant.fillPath.isEmpty()) {
                otherFills = otherFills.isEmpty() ? participant.fillPath : otherFills.united(participant.fillPath);
            }
        }
        // 剪除可能把底图切成互不相连的多块：每块单独成一个图元，
        // 这样每一块都能自己选中、拖动，而不是藏在一个图元里只能整体移动。
        appendRegions(base.path.subtracted(otherFills), baseIndex);
        break;
    }
    case Operation::LINE_SPLIT:
        // 已在函数入口转交 LineSplitOps 处理，不会走到这里。
        break;
    }

    // 极小碎块先吸收后删除（§5.3，D1/D2/D3/D15）。
    QList<MergeRegions::RegionBlock> blocks;
    for (const auto& raw : rawRegions) {
        MergeRegions::RegionBlock block;
        block.path = raw.first;
        block.area = PathRegions::Area(block.path);
        block.sourceIndex = raw.second;
        if (raw.second >= 0 && raw.second < participants.size()) {
            block.stackRank = participants.at(raw.second).stackRank;
        }
        block.exempt = IsBlockExempt(block.path, participants);
        blocks.append(block);
    }
    blocks = MergeRegions::AbsorbTinyFragments(blocks);

    // 允许区域掩膜（§5.2）：联合/XOR/相交/拆分为参与者填充并集；剪除为底图填充区。
    QPainterPath allowedMask;
    if (operation == Operation::SUBTRACT) {
        allowedMask = participants.at(baseIndex).fillPath;
    } else {
        for (const Participant& participant : participants) {
            if (participant.fillPath.isEmpty()) {
                continue;
            }
            allowedMask = allowedMask.isEmpty() ? participant.fillPath : allowedMask.united(participant.fillPath);
        }
    }

    // 结果图层：按连续块分别生成，填充/描线沿用原 BuildLayers 分层规则。
    QList<QList<PolygonItem::Layer>> results;
    QPainterPath finalFill;
    for (const MergeRegions::RegionBlock& block : blocks) {
        const QList<PolygonItem::Layer> layers =
            BuildLayers(block.path, participants, top, operation == Operation::INTERSECT);
        if (layers.isEmpty()) {
            continue;
        }
        results.append(layers);
        for (const PolygonItem::Layer& layer : layers) {
            if (layer.role == PolygonItem::LayerRole::FILLED_AREA && !layer.path.isEmpty()) {
                finalFill = finalFill.isEmpty() ? layer.path : finalFill.united(layer.path);
            }
        }
    }

    // 提交命令前完成校验：最终填充超出允许掩膜即不提交，不能事后裁剪或修补（§5.2、§12）。
    if (!MergeRegions::FillWithinMask(finalFill, allowedMask)) {
        return false;
    }

    if (results.isEmpty() && operation != Operation::SUBTRACT) {
        // D1：相交/联合/异或/拆分结果仅剩极小碎块并被删除时，本次合并不提交，保留原图元。
        return false;
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

    QUndoStack* undo = scene->UndoStack();
    if (!undo) {
        return false;
    }

    // 一次操作只占一条撤销记录；拆分产出的多个图元逐条加入同一宏内。
    QList<CanvasItem*> created;
    undo->beginMacro(commandText);
    undo->push(new RemoveItemsCommand(scene, originals, commandText));
    for (const QList<PolygonItem::Layer>& block : results) {
        auto* composite = new PolygonItem(block, top.fillStyle, top.strokeStyle);
        composite->SetName(scene->UniqueItemName(composite->TypeDisplayName()));
        composite->SetLayerId(top.item->LayerId());
        composite->SetZOrder(scene->NextHeight(top.item->LayerId()));
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
