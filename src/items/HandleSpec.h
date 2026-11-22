#pragma once

#include <QCursor>
#include <QList>
#include <QPointF>
#include <QtGlobal>

// 自定义句柄的行为策略：几何、命中与绘制留在公共基类，
// 句柄"拖了会怎样"由叶子给的策略决定，基类不再按图元类型分支。
enum class HandleBehavior { STATIC, RESIZE, ROTATE, ANGLE, SKEW, ARC_ENDPOINT };

// 一个自定义句柄的完整描述。position 是图元本地坐标，其余字段供视图层
// 决定命中优先级、光标与尺寸；sizeScale 相对通用手柄尺寸，1.0 为同尺寸。
struct HandleSpec {
    int id = -1;
    QPointF position;
    HandleBehavior behavior = HandleBehavior::STATIC;
    Qt::CursorShape cursor = Qt::SizeAllCursor;
    qreal sizeScale = 1.0;
    // 该句柄占用的位置与顶边缩放手柄冲突时（三角形顶点 C），置 true 让通用 TOP 手柄让位；
    // 默认 false，表示与八向手柄并存。
    bool replacesTopHandle = false;
};

using HandleSpecList = QList<HandleSpec>;
