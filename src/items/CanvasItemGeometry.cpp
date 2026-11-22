#include "items/CanvasItemGeometry.h"

#include <QtMath>

namespace CanvasItemGeometry {

QRectF ContentRect(const QSizeF& size)
{
    return QRectF(0.0, 0.0, size.width(), size.height());
}

QRectF BoundingRect(const QRectF& content, qreal framePadding, bool selected, qreal viewScale, qreal handleSize,
                    qreal rotationOffset)
{
    qreal topPadding = framePadding;
    if (selected) {
        // 旋转手柄画在内容区上方，其偏移随视图缩放变化。若 boundingRect
        // 不覆盖它，低缩放时 QGraphicsScene 的粗边界会先一步判定“未命中”，
        // 表现为旋转钮看得见却按不到、拖不动。
        const qreal knobRadius = handleSize / 2.0 + 2.0 / qMax<qreal>(viewScale, 0.001);
        topPadding = qMax(topPadding, rotationOffset + knobRadius);
    }
    return content.adjusted(-framePadding, -topPadding, framePadding, framePadding);
}

} // namespace CanvasItemGeometry
