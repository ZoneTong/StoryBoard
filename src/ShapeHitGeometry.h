#pragma once

#include "ShapeAbstractItem.h"

#include <QColor>
#include <QPainterPath>

// 图元几何与命中单元：把轮廓路径换算成可命中区域、可遮挡区域。只依赖路径与属性
// 快照，不访问图元本身；selectionHandleShape 由图元侧算好后传入合并。
class ShapeHitGeometry {
  public:
    // 命中区域：空心箭头取整剪影，描边类按笔画加宽，实心图形再叠一层轮廓描边。
    static QPainterPath HitShape(const QPainterPath& path, ShapeType type, qreal strokeWidth,
                                 const QPainterPath& handleShape);

    // 遮挡区域：描边扫过的部分始终遮挡；仅纯色填充才把轮廓内部整体算作遮挡区。
    // 描线样式为“无”（Qt::NoPen）时不加描边带，避免留下“看不见但点得到”的区域。
    static QPainterPath PaintedShape(const QPainterPath& path, ShapeType type, FillStyle fillStyle,
                                     const QColor& fillColor, qreal strokeWidth,
                                     Qt::PenStyle strokeStyle = Qt::SolidLine);
};
