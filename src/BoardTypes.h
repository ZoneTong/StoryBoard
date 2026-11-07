#pragma once

#include "I18n.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QString>

enum class ToolMode {
    View = 0,
    Brush = 1,
    Edit = 2,
    Text = 3
};

enum class BackgroundType {
    None = 0,
    Color = 1,
    Image = 2
};

enum class BackgroundMode {
    Stretch = 0,
    Fit = 1,
    Tile = 2
};

struct BackgroundState {
    BackgroundType type = BackgroundType::None;
    QColor color = QColor(255, 255, 255);
    QPixmap image;
    BackgroundMode mode = BackgroundMode::Stretch;

    bool operator==(const BackgroundState& other) const
    {
        if (type != other.type || color != other.color || mode != other.mode)
            return false;
        // QPixmap::cacheKey() only identifies the underlying pixel storage, so
        // two QPixmaps decoded from the same file compare unequal. Compare the
        // decoded contents instead, otherwise re-applying an identical image
        // pushes a redundant undo step.
        if (image.isNull() || other.image.isNull())
            return image.isNull() && other.image.isNull();
        if (image.size() != other.image.size())
            return false;
        return image.toImage() == other.image.toImage();
    }

    bool operator!=(const BackgroundState& other) const { return !(*this == other); }
};

struct BoardLayer {
    int id = 0;
    QString name;
    bool visible = true;
    bool locked = false;

    bool operator==(const BoardLayer& other) const
    {
        return id == other.id && name == other.name && visible == other.visible
            && locked == other.locked;
    }

    bool operator!=(const BoardLayer& other) const { return !(*this == other); }
};

struct LayerStackState {
    QList<BoardLayer> layers;
    int activeLayerId = 0;

    bool operator==(const LayerStackState& other) const
    {
        return layers == other.layers && activeLayerId == other.activeLayerId;
    }

    bool operator!=(const LayerStackState& other) const { return !(*this == other); }
};

inline QString toolModeName(ToolMode mode)
{
    switch (mode) {
    case ToolMode::View:
        return uiText(QStringLiteral("查看模式"));
    case ToolMode::Brush:
        return uiText(QStringLiteral("画笔模式"));
    case ToolMode::Edit:
        return uiText(QStringLiteral("编辑模式"));
    case ToolMode::Text:
        return uiText(QStringLiteral("文字模式"));
    }
    return QString();
}

inline QString backgroundModeName(BackgroundMode mode)
{
    switch (mode) {
    case BackgroundMode::Stretch:
        return uiText(QStringLiteral("拉伸铺满"));
    case BackgroundMode::Fit:
        return uiText(QStringLiteral("完整适应"));
    case BackgroundMode::Tile:
        return uiText(QStringLiteral("平铺"));
    }
    return QString();
}

inline QString backgroundModeToString(BackgroundMode mode)
{
    switch (mode) {
    case BackgroundMode::Stretch:
        return QStringLiteral("stretch");
    case BackgroundMode::Fit:
        return QStringLiteral("fit");
    case BackgroundMode::Tile:
        return QStringLiteral("tile");
    }
    return QStringLiteral("stretch");
}

inline BackgroundMode backgroundModeFromString(const QString& value)
{
    if (value == QStringLiteral("fit"))
        return BackgroundMode::Fit;
    if (value == QStringLiteral("tile"))
        return BackgroundMode::Tile;
    return BackgroundMode::Stretch;
}
