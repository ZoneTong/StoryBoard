#include "canvas/SelectionOverlayStyle.h"

#include <QPen>

namespace SelectionOverlayStyle {

namespace {
constexpr qreal KNOB_PIXELS = 9.0;
constexpr qreal KNOB_MIN = 6.0;
constexpr qreal KNOB_MAX = 40.0;
constexpr qreal ROTATION_GAP = 16.0;
constexpr qreal BORDER_STROKE = 1.3;
constexpr qreal KNOB_STROKE = 1.4;
} // namespace

qreal KnobSize(qreal viewScale)
{
    const qreal scale = viewScale > 0.0 ? viewScale : 1.0;
    return qBound(KNOB_MIN, KNOB_PIXELS / scale, KNOB_MAX);
}

QRectF KnobRect(const QPointF& center, qreal size)
{
    return QRectF(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
}

qreal RotationOffset(qreal viewScale)
{
    const qreal scale = viewScale > 0.0 ? viewScale : 1.0;
    return KnobSize(viewScale) + ROTATION_GAP / scale;
}

QPointF RotationKnobCenter(const QRectF& frame, qreal viewScale)
{
    return QPointF(frame.center().x(), frame.top() - RotationOffset(viewScale));
}

QColor HandleColor()
{
    return QColor(79, 124, 255);
}

QPen BorderPen()
{
    QPen pen(HandleColor());
    pen.setWidthF(BORDER_STROKE);
    pen.setCosmetic(true);
    pen.setStyle(Qt::DashLine);
    pen.setDashPattern({5.0, 4.0});
    return pen;
}

QPen KnobPen()
{
    QPen pen(HandleColor());
    pen.setWidthF(KNOB_STROKE);
    pen.setCosmetic(true);
    return pen;
}

} // namespace SelectionOverlayStyle
