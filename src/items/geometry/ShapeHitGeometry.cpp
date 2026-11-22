#include "items/geometry/ShapeHitGeometry.h"

#include <QPainterPathStroker>

QPainterPath ShapeHitGeometry::HitShape(const QPainterPath& path, ShapeType type, qreal strokeWidth,
                                        const QPainterPath& handleShape)
{
    if (path.isEmpty()) {
        return handleShape;
    }

    QPainterPath result;
    if (type == ShapeType::ARROW || type == ShapeType::DOUBLE_ARROW) {
        // Hollow arrows still use the whole silhouette as the hit area, so the
        // empty interior remains easy to select and drag.
        result = path;
    } else {
        // Filled shapes are stroked with strokeWidth, which paints up to
        // strokeWidth/2 outside the path. Without this ring the visible border
        // of a thick outline is not part of the hit area.
        QPainterPathStroker stroker;
        stroker.setWidth(qMax<qreal>(2.0, strokeWidth + 2.0));
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        result = stroker.createStroke(path);
        result.addPath(path);
    }

    result.addPath(handleShape);
    return result;
}

QPainterPath ShapeHitGeometry::PaintedShape(const QPainterPath& path, ShapeType type, FillStyle fillStyle,
                                            const QColor& fillColor, qreal strokeWidth, Qt::PenStyle strokeStyle)
{
    if (path.isEmpty()) {
        return QPainterPath();
    }

    // 描边本身也会遮挡下方图元；描线样式为“无”时整条描边不画，也就不参与遮挡。
    QPainterPath result;
    if (strokeStyle != Qt::NoPen) {
        QPainterPathStroker stroker;
        stroker.setWidth(qMax<qreal>(1.0, strokeWidth));
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        result = stroker.createStroke(path);
    }

    // 有面积的图形只有“纯色填充”时才会完整遮住下层：纯色（含半透明纯色）的轮廓内部
    // 整个算作遮挡区，下层颜色完全不画，避免叠出混合色。
    // 无填充、斜线/交叉线/六边形等纹理填充不遮挡下层的轮廓，下层照常可见（查看模式也一样）；
    // 描边本身仍然遮挡笔画扫过的区域。
    const bool solidFill = fillStyle == FillStyle::SOLID && fillColor.alpha() > 0;
    if (solidFill) {
        result = result.united(path);
    }
    return result;
}
