#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>

class QPen;

// 选中框（单图元与组）共用的绘制度量（D1）。屏幕上的手柄尺寸只由这里决定，
// 单图元在局部坐标里除以缩放、组框在视口坐标里直接取值，两边永远不会各写一套。
namespace SelectionOverlayStyle {

// 手柄边长：按视图缩放折算，保证屏幕上恒定（6–40 像素之间夹住）。
qreal KnobSize(qreal viewScale);

// 以中心点为准的方形手柄区域；旧代码用 corner 对齐会让手柄整体偏 (size/2)。
QRectF KnobRect(const QPointF& center, qreal size);

// 旋转钮相对框顶边的距离（局部单位）。
qreal RotationOffset(qreal viewScale);

// 旋转钮中心：框顶边中点正上方。
QPointF RotationKnobCenter(const QRectF& frame, qreal viewScale);

QColor HandleColor();
QPen BorderPen();
QPen KnobPen();

} // namespace SelectionOverlayStyle
