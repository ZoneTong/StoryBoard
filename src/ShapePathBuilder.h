#pragma once

#include "ShapeItem.h"

#include <QPainterPath>
#include <QRectF>

// 按 ShapeType 生成轮廓路径的纯几何单元：不持有图元状态，只根据类型与矩形
// （含圆角、扇形 / 弧线的起始角、扫过角与半径参数）计算 QPainterPath；
// 自定义模板路径的单位坐标铺开也在这里。
class ShapePathBuilder {
  public:
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle, qreal shapeRadius);

    // 模板存的是 0–1 单位坐标，按内容矩形铺开，路径本身可任意放大缩小。
    static QPainterPath mapUnitPath(const QPainterPath& unitPath, const QRectF& rect);
};
