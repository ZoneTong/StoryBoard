#pragma once

#include "items/properties/FillAttributes.h"

#include <QColor>
#include <QList>
#include <QPainterPath>
#include <Qt>
#include <climits>

class CanvasItem;
class CanvasScene;

// 合并形状运算的内部参与者：描述一个待合并图元（整组时为组内全部成员）的
// 场景轮廓、样式来源与叠放信息。仅 BoolOps 系列翻译单元共用，不对外暴露。
struct BoolOpsParticipant {
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

// 收集参与运算的参与者：先记录合并前叠放序号（绘制顺序用），再按样式优先级
// 从低到高排序（有填充空间整体靠后、优先级高，末位即 donor）。整组只算一个
// 参与者，避免组内成员被拆开重复参与。
QList<BoolOpsParticipant> CollectParticipants(CanvasScene* scene);
