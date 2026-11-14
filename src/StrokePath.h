#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QVector>

// 由笔画采样点生成 QPainterPath：单点成圆点、折线走直线段、曲线走二次贝塞尔角切；
// 另提供折线长度计算。只负责路径几何，不保存笔画数据。
namespace StrokePath {

QPainterPath build(const QVector<QPointF>& points, bool dot, bool linearPath, qreal penWidth);
qreal length(const QVector<QPointF>& points);

} // namespace StrokePath
