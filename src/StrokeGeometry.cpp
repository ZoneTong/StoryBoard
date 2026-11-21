#include "StrokeGeometry.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>
#include <QtMath>

namespace StrokeGeometry {

namespace {

qreal averageScale(const QSizeF& baseSize, const QSizeF& contentSize)
{
    const qreal scaleX = contentSize.width() / baseSize.width();
    const qreal scaleY = contentSize.height() / baseSize.height();
    return (scaleX + scaleY) / 2.0;
}

} // namespace

QPainterPath outline(const QPainterPath& path, const QSizeF& baseSize, const QSizeF& contentSize)
{
    if (baseSize.isEmpty() || path.isEmpty()) {
        return QPainterPath();
    }

    QTransform transform;
    transform.scale(contentSize.width() / baseSize.width(), contentSize.height() / baseSize.height());
    return transform.map(path);
}

QPainterPath Painted(const QPainterPath& outline, const QSizeF& baseSize, const QSizeF& contentSize, qreal penWidth)
{
    if (outline.isEmpty()) {
        return outline;
    }

    const qreal average = averageScale(baseSize, contentSize);
    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(1.0, penWidth * average));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(outline);
}

QPainterPath HitScaled(const QPainterPath& path, const QSizeF& baseSize, const QSizeF& contentSize, qreal penWidth,
                       qreal viewScale)
{
    if (baseSize.isEmpty()) {
        return QPainterPath();
    }

    const qreal scaleX = contentSize.width() / baseSize.width();
    const qreal scaleY = contentSize.height() / baseSize.height();
    QTransform transform;
    transform.scale(scaleX, scaleY);
    const QPainterPath scaled = transform.map(path);

    QPainterPathStroker stroker;
    const qreal averageScaleValue = (scaleX + scaleY) / 2.0;
    const qreal tolerance = qMin<qreal>(4.0 / viewScale, 16.0);
    stroker.setWidth(qMax<qreal>(2.0, penWidth * averageScaleValue) + tolerance);
    QPainterPath result = stroker.createStroke(scaled);
    result.addPath(scaled);
    return result;
}

void render(QPainter* painter, const QPainterPath& path, const QSizeF& baseSize, const QSizeF& contentSize,
            const QColor& color, qreal penWidth, Qt::PenStyle style)
{
    if (baseSize.isEmpty() || path.isEmpty()) {
        return;
    }

    const qreal scaleX = contentSize.width() / baseSize.width();
    const qreal scaleY = contentSize.height() / baseSize.height();

    painter->save();
    painter->scale(scaleX, scaleY);
    QPen pen(color);
    pen.setWidthF(penWidth);
    pen.setStyle(style);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
    painter->restore();
}

} // namespace StrokeGeometry
