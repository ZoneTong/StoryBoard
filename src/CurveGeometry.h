#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QVector>

// 曲线图元的最小几何单元：一段三次贝塞尔。
struct CurveSegment {
    QPointF p0;
    QPointF c1;
    QPointF c2;
    QPointF p1;

    bool operator==(const CurveSegment& other) const;
    bool operator!=(const CurveSegment& other) const { return !(*this == other); }
};

// 曲线几何：把绘制期采样点拟合成分段三次贝塞尔，并把贝塞尔还原成折线。
namespace CurveGeometry {

// 采样点 → 贝塞尔段。tolerance > 0 时先做 RDP 抽点，控制点数量随容差增大而下降；
// linear 为 true 时每段退化成直线（图片线稿化使用）。
QVector<CurveSegment> fitSegments(QVector<QPointF> points, qreal tolerance, bool linear);

QPainterPath buildPath(const QVector<CurveSegment>& segments);
QPainterPath buildDotPath(const QPointF& center, qreal radius);

// 贝塞尔段的端点序列（首段起点 + 每段终点），用于撤销比较与外部只读访问。
QVector<QPointF> knots(const QVector<CurveSegment>& segments);

// 把已有路径拍平成折线，供“简化曲线”重新拟合。
QVector<QPointF> flatten(const QPainterPath& path);

} // namespace CurveGeometry
