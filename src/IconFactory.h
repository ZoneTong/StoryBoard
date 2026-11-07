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
    View,
    Brush,
    Edit,
    Text,
    Fit,
    ZoomIn,
    ZoomOut,
    Image,
    Background,
    Settings,
    Delete,
    Grid,
    Palette,
    Folder,
    Shapes
};

QIcon makeIcon(IconId id, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
QIcon makeColorIcon(const QColor& color, int logicalSize = 18);
