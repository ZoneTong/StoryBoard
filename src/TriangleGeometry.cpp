#include "TriangleGeometry.h"

#include <QtMath>

namespace {

qreal Cot(qreal degrees)
{
    return 1.0 / qTan(qDegreesToRadians(degrees));
}

} // namespace

void TriangleGeometry::ClampAngles(qreal* angle1, qreal* angle2)
{
    const qreal a1 = qBound<qreal>(5.0, *angle1, 170.0);
    const qreal a2 = qBound<qreal>(5.0, *angle2, 175.0 - a1);
    *angle1 = a1;
    *angle2 = a2;
}

qreal TriangleGeometry::AspectRatio(qreal angle1, qreal angle2)
{
    ClampAngles(&angle1, &angle2);
    const qreal cot1 = Cot(angle1);
    const qreal cot2 = Cot(angle2);
    if (cot1 <= 0.0) {
        return qMax<qreal>(0.02, cot2);
    }
    if (cot2 <= 0.0) {
        return qMax<qreal>(0.02, cot1);
    }
    return qMax<qreal>(0.02, cot1 + cot2);
}

TriangleGeometry::Vertices TriangleGeometry::Make(const QRectF& frame, qreal angle1, qreal angle2)
{
    const QRectF box = frame.normalized();
    Vertices result;
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return result;
    }
    ClampAngles(&angle1, &angle2);
    const qreal height = box.height();
    const qreal cot1 = Cot(angle1);
    const qreal cot2 = Cot(angle2);
    const qreal cFromLeft = height * cot1;
    const qreal bFromLeft = height * (cot1 + cot2);
    const qreal shift = -qMin<qreal>(0.0, qMin(cFromLeft, bFromLeft));
    result.a = QPointF(box.left() + shift, box.bottom());
    result.c = QPointF(box.left() + shift + cFromLeft, box.top());
    result.b = QPointF(box.left() + shift + bFromLeft, box.bottom());
    result.baseWidth = height * (cot1 + cot2);
    result.frameWidth = height * AspectRatio(angle1, angle2);
    return result;
}

QSizeF TriangleGeometry::SizeForAngles(const QSizeF& oldSize, qreal oldAngle1, qreal oldAngle2, qreal angle1,
                                       qreal angle2)
{
    ClampAngles(&oldAngle1, &oldAngle2);
    ClampAngles(&angle1, &angle2);
    const qreal oldSum = qMax<qreal>(0.02, Cot(oldAngle1) + Cot(oldAngle2));
    const qreal baseWidth = qMax<qreal>(1.0, oldSize.height()) * oldSum;
    const qreal newSum = qMax<qreal>(0.02, Cot(angle1) + Cot(angle2));
    const qreal height = baseWidth / newSum;
    return QSizeF(height * AspectRatio(angle1, angle2), height);
}

QSizeF TriangleGeometry::SizeForHeight(qreal height, qreal angle1, qreal angle2)
{
    const qreal safeHeight = qMax<qreal>(1.0, height);
    return QSizeF(safeHeight * AspectRatio(angle1, angle2), safeHeight);
}

QPainterPath TriangleGeometry::Path(const QRectF& frame, qreal angle1, qreal angle2)
{
    const QRectF box = frame.normalized();
    QPainterPath path;
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return path;
    }
    const Vertices vertices = Make(box, angle1, angle2);
    const qreal scaleX = vertices.frameWidth > 0.0 ? box.width() / vertices.frameWidth : 1.0;
    auto mapped = [&box, scaleX](const QPointF& point) {
        return QPointF(box.left() + (point.x() - box.left()) * scaleX, point.y());
    };
    path.moveTo(mapped(vertices.c));
    path.lineTo(mapped(vertices.a));
    path.lineTo(mapped(vertices.b));
    path.closeSubpath();
    return path;
}
