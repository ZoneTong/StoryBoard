#include "items/geometry/VertexAngleKnob.h"

#include <QTransform>
#include <QtMath>

namespace {
constexpr qreal GAP_PIXELS = 34.0;
constexpr qreal MIN_GAP_PIXELS = 6.0;
constexpr qreal SHAPE_RATIO = 0.7;
} // namespace

qreal VertexAngleKnob::GapPixels()
{
    return GAP_PIXELS;
}

QPointF VertexAngleKnob::Position(const QPointF& vertex, const QPointF& neighborA, const QPointF& neighborB,
                                  qreal viewScale, const QRectF& content)
{
    const qreal scale = qMax<qreal>(viewScale, 0.001);
    const QPointF bisector = (neighborA - vertex) + (neighborB - vertex);
    const qreal length = qSqrt(bisector.x() * bisector.x() + bisector.y() * bisector.y());
    if (length < 1.0e-6) {
        return vertex;
    }
    // 间距按屏幕像素换算（缩放时旋钮大小与离顶点的距离都不变），再按图元短边收一档，
    // 免得扁 / 小的图元把旋钮顶到对面边上。
    qreal gap = qMin<qreal>(GAP_PIXELS / scale, SHAPE_RATIO * qMin(content.width(), content.height()));
    gap = qMax<qreal>(gap, MIN_GAP_PIXELS / scale);
    return vertex + bisector / length * gap;
}

qreal VertexAngleKnob::PointerAngleDegrees(const QPointF& vertexScene, const QPointF& pointerScene,
                                           qreal rotationDegrees)
{
    QPointF delta = pointerScene - vertexScene;
    if (!qFuzzyIsNull(rotationDegrees)) {
        QTransform transform;
        transform.rotate(-rotationDegrees);
        delta = transform.map(delta);
    }
    // 图元本地 y 轴向下，这里按“向上为正”的数学角度计量，锐角 / 钝角连续。
    return qRadiansToDegrees(qAtan2(-delta.y(), delta.x()));
}
