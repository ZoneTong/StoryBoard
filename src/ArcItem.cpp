#include "ArcItem.h"

#include <QJsonArray>
#include <QVector>

namespace {

constexpr qreal MINIMUM_FRAME_SIDE = 12.0;
constexpr int ARC_SAMPLES = 64;

QRectF PaddedToMinimum(QRectF frame)
{
    if (frame.width() < MINIMUM_FRAME_SIDE) {
        frame.moveLeft(frame.center().x() - MINIMUM_FRAME_SIDE / 2.0);
        frame.setWidth(MINIMUM_FRAME_SIDE);
    }
    if (frame.height() < MINIMUM_FRAME_SIDE) {
        frame.moveTop(frame.center().y() - MINIMUM_FRAME_SIDE / 2.0);
        frame.setHeight(MINIMUM_FRAME_SIDE);
    }
    return frame;
}

QJsonArray RectToArray(const QRectF& rect)
{
    QJsonArray array;
    array.append(rect.x());
    array.append(rect.y());
    array.append(rect.width());
    array.append(rect.height());
    return array;
}

QRectF ArrayToRect(const QJsonArray& array, const QRectF& fallback)
{
    if (array.size() < 4) {
        return fallback;
    }
    return QRectF(array.at(0).toDouble(), array.at(1).toDouble(), array.at(2).toDouble(), array.at(3).toDouble());
}

} // namespace

ArcItem::ArcItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

ItemPropertySet ArcItem::Properties() const
{
    // 弧线只有角度参数，没有箭头行（与现状一致）。
    return LineItemAbstract::Properties().United(ArcAngleAttributes::ArcAngleProperties());
}

QVariant ArcItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        return StartAngle();
    case ItemProperty::SWEEP_ANGLE:
        return SweepAngle();
    default:
        return LineItemAbstract::PropertyValue(property);
    }
}

void ArcItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        SetAngles(value.toDouble(), SweepAngle());
        break;
    case ItemProperty::SWEEP_ANGLE:
        SetAngles(StartAngle(), value.toDouble());
        break;
    default:
        LineItemAbstract::SetPropertyValue(property, value);
        break;
    }
}

QString ArcItem::DisplayName() const
{
    return UiText(QStringLiteral("Arc"));
}

QRectF ArcItem::FrameRect() const
{
    return PaddedToMinimum(CanvasItem::FrameRect());
}

ArcItem::ParamState ArcItem::paramState() const
{
    return ParamState{StartAngle(), SweepAngle(), true};
}

void ArcItem::ApplyParamState(const ParamState& state)
{
    SetAngles(state.first, state.second);
}

void ArcItem::SetAngles(qreal startDeg, qreal sweepDeg)
{
    ArcAngleAttributes::SetArcAngles(startDeg, EllipseShape::ClampedSweep(sweepDeg));
}

void ArcItem::SetArcAngles(qreal startDeg, qreal sweepDeg)
{
    SetAngles(startDeg, sweepDeg);
}

void ArcItem::ArcAnglesChanged()
{
    Rebuild();
}

void ArcItem::SetArcGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                             const QColor& color, qreal width)
{
    m_startAngle = startDeg;
    m_sweepAngle = EllipseShape::ClampedSweep(sweepDeg);

    const QRectF frameScene(topLeftScene, size);
    SetFrame(frameScene);
    SetStrokePoints(Sample(m_startAngle, m_sweepAngle, ARC_SAMPLES), color, width, 0.0, false);

    const QPointF origin = pos();
    const QSizeF box = ContentSize();
    if (box.width() > 0.0 && box.height() > 0.0) {
        m_refFrame =
            QRectF((frameScene.left() - origin.x()) / box.width(), (frameScene.top() - origin.y()) / box.height(),
                   frameScene.width() / box.width(), frameScene.height() / box.height());
    } else {
        m_refFrame = QRectF(0.0, 0.0, 1.0, 1.0);
    }
}

void ArcItem::Rebuild()
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_refFrame.x() * box.width(), m_refFrame.y() * box.height(), m_refFrame.width() * box.width(),
                       m_refFrame.height() * box.height());
    const QRectF frameScene = local.translated(pos());
    SetFrame(frameScene);
    SetStrokePoints(Sample(m_startAngle, m_sweepAngle, ARC_SAMPLES), StrokeColor(), StrokeWidth(), 0.0, false);

    const QPointF origin = pos();
    const QSizeF now = ContentSize();
    if (now.width() > 0.0 && now.height() > 0.0) {
        m_refFrame =
            QRectF((frameScene.left() - origin.x()) / now.width(), (frameScene.top() - origin.y()) / now.height(),
                   frameScene.width() / now.width(), frameScene.height() / now.height());
    }
    update();
}

QJsonObject ArcItem::ToJson() const
{
    QJsonObject json = LineItemAbstract::ToJson();
    WriteArcAngleJson(json);
    json.insert(QStringLiteral("arcFrame"), RectToArray(m_refFrame));
    return json;
}

void ArcItem::FromJson(const QJsonObject& json)
{
    LineItemAbstract::FromJson(json);
    ReadArcAngleJson(json);
    m_refFrame = ArrayToRect(json.value(QStringLiteral("arcFrame")).toArray(), QRectF(0.0, 0.0, 1.0, 1.0));
    Rebuild();
}
