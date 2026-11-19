#include "LineItem.h"

#include <QVector>

namespace {

constexpr qreal kMinimumFrameSide = 12.0;

QRectF paddedToMinimum(QRectF frame)
{
    if (frame.width() < kMinimumFrameSide) {
        frame.moveLeft(frame.center().x() - kMinimumFrameSide / 2.0);
        frame.setWidth(kMinimumFrameSide);
    }
    if (frame.height() < kMinimumFrameSide) {
        frame.moveTop(frame.center().y() - kMinimumFrameSide / 2.0);
        frame.setHeight(kMinimumFrameSide);
    }
    return frame;
}

} // namespace

LineItem::LineItem(QGraphicsItem* parent) : CurveItem(parent)
{
    setCurveKind(CurveKind::Line);
}

QString LineItem::displayName() const
{
    return arrowStyleIsHead(startArrow()) || arrowStyleIsHead(endArrow()) ? uiText(QStringLiteral("Line arrow"))
                                                                         : uiText(QStringLiteral("Line"));
}

QRectF LineItem::frameRect() const
{
    return paddedToMinimum(CanvasItem::frameRect());
}

void LineItem::setEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color, qreal width)
{
    setStrokePoints(QVector<QPointF>{startScene, endScene}, color, width, 0.0, true);
}
