#pragma once

#include "app/I18n.h"

#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QString>

enum class ToolMode { VIEW = 0, BRUSH = 1, EDIT = 2, TEXT = 3 };

enum class BackgroundType { NONE = 0, COLOR = 1, IMAGE = 2 };

enum class BackgroundMode { STRETCH = 0, FIT = 1, TILE = 2 };

// 分组动作失败原因：CanGroupSelectedItems / GroupSelectedItems / AssignNewGroup
// 通过出参写出，状态栏按枚举给出对应提示文案。
enum class GroupFailureReason {
    NONE = 0,
    NOT_EDIT_MODE,
    NOT_ENOUGH_ITEMS,
    NOT_EDITABLE,
    DIFFERENT_LAYERS,
    INVALID_ITEMS
};

struct CanvasBackground {
    BackgroundType type = BackgroundType::NONE;
    QColor color = QColor(255, 255, 255);
    QPixmap image;
    BackgroundMode mode = BackgroundMode::STRETCH;

    bool operator==(const CanvasBackground& other) const
    {
        if (type != other.type || color != other.color || mode != other.mode) {
            return false;
        }
        // QPixmap::CacheKey() only identifies the underlying pixel storage, so
        // two QPixmaps decoded from the same file compare unequal. Compare the
        // decoded contents instead, otherwise re-applying an identical image
        // pushes a redundant undo step.
        if (image.isNull() || other.image.isNull()) {
            return image.isNull() && other.image.isNull();
        }
        if (image.size() != other.image.size()) {
            return false;
        }
        return image.toImage() == other.image.toImage();
    }

    bool operator!=(const CanvasBackground& other) const { return !(*this == other); }
};

struct CanvasLayer {
    int id = 0;
    QString name;
    bool visible = true;
    bool locked = false;

    bool operator==(const CanvasLayer& other) const
    {
        return id == other.id && name == other.name && visible == other.visible && locked == other.locked;
    }

    bool operator!=(const CanvasLayer& other) const { return !(*this == other); }
};

struct LayerStackState {
    QList<CanvasLayer> layers;
    int activeLayerId = 0;

    bool operator==(const LayerStackState& other) const
    {
        return layers == other.layers && activeLayerId == other.activeLayerId;
    }

    bool operator!=(const LayerStackState& other) const { return !(*this == other); }
};

inline QString ToolModeName(ToolMode mode)
{
    switch (mode) {
    case ToolMode::VIEW:
        return QCoreApplication::tr("View mode");
    case ToolMode::BRUSH:
        return QCoreApplication::tr("Brush mode");
    case ToolMode::EDIT:
        return QCoreApplication::tr("Edit mode");
    case ToolMode::TEXT:
        return QCoreApplication::tr("Text mode");
    }
    return QString();
}

inline QString BackgroundModeName(BackgroundMode mode)
{
    switch (mode) {
    case BackgroundMode::STRETCH:
        return QCoreApplication::tr("Stretch to fill");
    case BackgroundMode::FIT:
        return QCoreApplication::tr("Fit entirely");
    case BackgroundMode::TILE:
        return QCoreApplication::tr("Tile");
    }
    return QString();
}

inline QString BackgroundModeToString(BackgroundMode mode)
{
    switch (mode) {
    case BackgroundMode::STRETCH:
        return QStringLiteral("stretch");
    case BackgroundMode::FIT:
        return QStringLiteral("fit");
    case BackgroundMode::TILE:
        return QStringLiteral("tile");
    }
    return QStringLiteral("stretch");
}

inline BackgroundMode BackgroundModeFromString(const QString& value)
{
    if (value == QStringLiteral("fit")) {
        return BackgroundMode::FIT;
    }
    if (value == QStringLiteral("tile")) {
        return BackgroundMode::TILE;
    }
    return BackgroundMode::STRETCH;
}

inline QString GroupFailureReasonText(GroupFailureReason reason)
{
    switch (reason) {
    case GroupFailureReason::NONE:
        return QString();
    case GroupFailureReason::NOT_EDIT_MODE:
        return QCoreApplication::tr("Switch to edit mode before creating a group");
    case GroupFailureReason::NOT_ENOUGH_ITEMS:
        return QCoreApplication::tr("Select at least two items to create a group");
    case GroupFailureReason::NOT_EDITABLE:
        return QCoreApplication::tr("The selection contains non-editable or locked items");
    case GroupFailureReason::DIFFERENT_LAYERS:
        return QCoreApplication::tr("Items in a group must belong to the same layer");
    case GroupFailureReason::INVALID_ITEMS:
        return QCoreApplication::tr("The selection is no longer valid; please select again");
    }
    return QString();
}
