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
    // 合并形状五项操作，图形参照 PPT「合并形状」菜单。
    MergeUnion,
    MergeCombine,
    MergeFragment,
    MergeIntersect,
    MergeSubtract,
    // 多选工具卡里的对齐入口。
    Align
};

QIcon makeIcon(IconId id, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
QIcon makeColorIcon(const QColor& color, int logicalSize = 18);
