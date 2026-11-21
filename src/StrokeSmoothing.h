#pragma once

#include <QPointF>
#include <QVector>

// 笔画采样点的简化与平滑：RDP 抽点、Chaikin 角切、直线吸附判定，
// 以及撤销快照用到的点集近似相等比较。算法不持有笔画数据。
namespace StrokeSmoothing {

// 撤销/重做快照比较时允许的坐标误差。
constexpr qreal STATE_EPSILON = 0.01;

bool SamePoint(const QPointF& first, const QPointF& second);
bool SamePoints(const QVector<QPointF>& first, const QVector<QPointF>& second);

// Ramer-Douglas-Peucker：容差越大，越多的小抖动会被压掉。
QVector<QPointF> SimplifyRdp(const QVector<QPointF>& points, qreal tolerance);
QVector<QPointF> ChaikinSmooth(const QVector<QPointF>& points, int iterations);

// 判定是否接近直线；chordLength 回填首尾弦长。
bool LooksLikeStraightLine(const QVector<QPointF>& points, qreal tolerance, qreal& chordLength);

} // namespace StrokeSmoothing
