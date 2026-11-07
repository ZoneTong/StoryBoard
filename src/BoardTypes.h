#pragma once

#include <QColor>
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
        return type == other.type
            && color == other.color
            && mode == other.mode
            && image.cacheKey() == other.image.cacheKey();
    }

    bool operator!=(const BackgroundState& other) const { return !(*this == other); }
};

struct BoardLayer {
    int id = 0;
    QString name;
    bool visible = true;

    bool operator==(const BoardLayer& other) const
    {
        return id == other.id && name == other.name && visible == other.visible;
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
        return QStringLiteral("查看模式");
    case ToolMode::Brush:
        return QStringLiteral("画笔模式");
    case ToolMode::Edit:
        return QStringLiteral("编辑模式");
    case ToolMode::Text:
        return QStringLiteral("文字模式");
    }
    return QString();
}

inline QString backgroundModeName(BackgroundMode mode)
{
    switch (mode) {
    case BackgroundMode::Stretch:
        return QStringLiteral("拉伸铺满");
    case BackgroundMode::Fit:
        return QStringLiteral("完整适应");
    case BackgroundMode::Tile:
        return QStringLiteral("平铺");
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
