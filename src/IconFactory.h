#pragma once

#include <QColor>
#include <QIcon>

enum class IconId {
    NEW,
    OPEN,
    SAVE,
    EXPORT,
    UNDO,
    REDO,
    ROTATE_CCW,
    ROTATE_CW,
    VIEW,
    BRUSH,
    EDIT,
    TEXT,
    FIT,
    ZOOM_IN,
    ZOOM_OUT,
    IMAGE,
    LINE_ART,
    BACKGROUND,
    SETTINGS,
    DELETE,
    GRID,
    PALETTE,
    FOLDER,
    SHAPES,
    SMOOTH,
    LOCK,
    // 编辑面板用的紧凑图标。
    LAYER_UP,
    LAYER_DOWN,
    ORDER_UP,
    ORDER_DOWN,
    CORNER,
    OPACITY,
    // 三角形两个底角：开口朝右的角（底角 A）与开口朝左的角（底角 B）。
    ANGLE_RIGHT,
    ANGLE_LEFT,
    // 直线两端箭头：起点朝左、终点朝右，避免两行都用旋转图标而分不清。
    ARROW_START,
    ARROW_END,
    // 正弦波形两行：周期数是波峰下带左右双向箭头（沿轴往复推进），振幅是波峰右侧带上下双向箭头（波高）。
    SINE_CYCLES,
    SINE_AMPLITUDE,
    // 合并形状五项操作，图形参照 PPT「合并形状」菜单。
    MERGE_UNION,
    MERGE_COMBINE,
    MERGE_FRAGMENT,
    MERGE_INTERSECT,
    MERGE_SUBTRACT,
    // 线条分割：两个交叠圆被一条斜刀线切开，一侧实心表示分割结果。
    MERGE_LINE_SPLIT,
    // 多选工具卡里的对齐入口。
    ALIGN,
    // 对齐展开后的 8 个按钮：左 / 水平居中 / 右 / 上 / 垂直居中 / 下 / 水平等距 / 垂直等距。
    ALIGN_LEFT,
    ALIGN_CENTER_H,
    ALIGN_RIGHT,
    ALIGN_TOP,
    ALIGN_CENTER_V,
    ALIGN_BOTTOM,
    DISTRIBUTE_H,
    DISTRIBUTE_V,
    // 选中操作栏：复制、组合、取消组合；合并图元主入口用独立的形变图标，避免和五项子操作重复。
    COPY,
    GROUP,
    UNGROUP,
    MORPH
};

QIcon MakeIcon(IconId id, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
QIcon MakeColorIcon(const QColor& color, int logicalSize = 18);
