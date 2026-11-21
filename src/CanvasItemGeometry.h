#pragma once

#include <QRectF>
#include <QSizeF>

// 图元内容矩形与包围矩形换算：内容矩形由逻辑尺寸给出，包围矩形在四周预留
// 帧边距，选中时还要给上方的旋转手柄留出位置。
namespace CanvasItemGeometry {

QRectF ContentRect(const QSizeF& size);

// 依据内容矩形、帧边距与选中状态计算图元的粗包围矩形。
QRectF boundingRect(const QRectF& content, qreal framePadding, bool selected, qreal viewScale, qreal handleSize,
                    qreal rotationOffset);

} // namespace CanvasItemGeometry
