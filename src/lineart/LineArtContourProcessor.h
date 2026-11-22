#pragma once

#include "lineart/LineArtConverter.h"

#include <QPointF>
#include <QVector>

namespace LineArtInternal {

// Moore 邻域边界跟踪：从色块最左上像素出发绕一圈。
// 返回像素中心坐标，首点只出现一次（闭合点由 buildOutlinePoints 补上）。
QVector<QPointF> TraceContour(const QVector<int>& labels, int width, int height, int seed, int regionId);

// 迭代版 Ramer-Douglas-Peucker 抽稀：像素阶梯轮廓压成少量折线段。
QVector<QPointF> SimplifyContour(const QVector<QPointF>& points, qreal tolerance);

// 按平均宽度过滤极细条带，抽稀后补闭合点；不满足描边条件时返回空点列。
QVector<QPointF> BuildOutlinePoints(const QVector<QPointF>& contour, int area,
                                    const LineArtConverter::Options& options);

} // namespace LineArtInternal
