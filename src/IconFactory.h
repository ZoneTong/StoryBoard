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
    Lock
};

QIcon makeIcon(IconId id, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
QIcon makeColorIcon(const QColor& color, int logicalSize = 18);
