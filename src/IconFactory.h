#pragma once

#include <QColor>
#include <QIcon>

enum class IconId {
    New,
    Open,
    Save,
    Export,
    Undo,
    Redo,
    RotateCcw,
    RotateCw,
    View,
    Brush,
    Edit,
    Text,
    Fit,
    ZoomIn,
    ZoomOut,
    Image,
    LineArt,
    Background,
    Settings,
    Delete,
    Grid,
    Palette,
    Folder,
    Shapes,
    Smooth,
    Lock,
    // 编辑图元卡用的紧凑图标。
    LayerUp,
    LayerDown,
    OrderUp,
    OrderDown,
    Corner,
    Opacity,
    // 三角形底边长度：底边加两端内角、不闭合的开口轮廓。
    TriangleBase,
    // 三角形两个底角：开口朝右的角（底角 A）与开口朝左的角（底角 B）。
    AngleRight,
    AngleLeft,
    // 直线两端箭头：起点朝左、终点朝右，避免两行都用旋转图标而分不清。
    ArrowStart,
    ArrowEnd,
    // 合并形状五项操作，图形参照 PPT「合并形状」菜单。
    MergeUnion,
    MergeCombine,
    MergeFragment,
    MergeIntersect,
    MergeSubtract,
    // 线条分割：两个交叠圆被一条斜刀线切开，一侧实心表示分割结果。
    MergeLineSplit,
    // 多选工具卡里的对齐入口。
    Align,
    // 对齐展开后的 8 个按钮：左 / 水平居中 / 右 / 上 / 垂直居中 / 下 / 水平等距 / 垂直等距。
    AlignLeft,
    AlignCenterH,
    AlignRight,
    AlignTop,
    AlignCenterV,
    AlignBottom,
    DistributeH,
    DistributeV,
    // 选中操作栏：复制、组合、取消组合；合并图元主入口用独立的形变图标，避免和五项子操作重复。
    Copy,
    Group,
    Ungroup,
    Morph
};

QIcon makeIcon(IconId id, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
QIcon makeColorIcon(const QColor& color, int logicalSize = 18);
