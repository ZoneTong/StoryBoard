#include "ops/BoolOpsParticipant.h"

#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/polygon/PolygonAccess.h"
#include "items/polygon/PolygonItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "ops/PathRegions.h"
#include "scene/CanvasScene.h"

#include <QPainterPath>
#include <QSet>
#include <algorithm>
#include <climits>

namespace {

// 线条类图元不参与合并：直线（含直线箭头）、括号、弧、曲线，以及纯描线多边形。
bool IsLineClassItem(CanvasItem* item)
{
    if (dynamic_cast<LineAbstractItem*>(item)) {
        return true;
    }
    // 零面积图元已全部并入曲线族，普通形状类恒有填充面积；多边形要看图层。
    if (PolygonItem* polygon = PolygonAccess::As(item)) {
        return !polygon->HasFillAreaLayer();
    }
    return false;
}

// 判定图元是否「有填充空间」：闭合图形与含面积层的多边形算有，直线 / 曲线 /
// 文字是零面积描线，算没有。只看几何类型，不看当前填充样式——无填充的闭合
// 图形仍按完整轮廓参与布尔运算，也应优先决定合并结果的样式。
bool HasFillSpace(CanvasItem* item)
{
    if (PolygonItem* polygon = PolygonAccess::As(item)) {
        return polygon->HasFillAreaLayer();
    }
    return !dynamic_cast<LineAbstractItem*>(item) && item->Category() != ItemCategory::TEXT;
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
                       QList<BoolOpsParticipant>* out)
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

    BoolOpsParticipant participant;
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
        } else if (PolygonItem* polygon = PolygonAccess::As(vectorMember)) {
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
        participant.filled = PolygonAccess::As(styleItem) || shape->fillStyle() != FillStyle::NONE;
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

} // namespace

QList<BoolOpsParticipant> CollectParticipants(CanvasScene* scene)
{
    QList<BoolOpsParticipant> participants;
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
    std::sort(participants.begin(), participants.end(), [](const BoolOpsParticipant& left, const BoolOpsParticipant& right) {
        if (left.fillSpace != right.fillSpace) {
            return !left.fillSpace;
        }
        return left.stackRank < right.stackRank;
    });
    return participants;
}
