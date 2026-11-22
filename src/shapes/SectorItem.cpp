#include "shapes/SectorItem.h"

#include <QJsonObject>

SectorItem::SectorItem()
{
    SetContentSize(DefaultContentSize());
    SetFrame(ContentRect());
}

void SectorItem::ContentResized()
{
    ShapeAbstractItem::ContentResized();
    // 外接框与内容矩形保持一致，扇形只由 EllipseShape::Sector() 出。
    SetFrame(ContentRect());
}

ItemPropertySet SectorItem::Properties() const
{
    return ShapeAbstractItem::Properties().United(ArcAngleAttributes::ArcAngleProperties());
}

QVariant SectorItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        return StartAngle();
    case ItemProperty::SWEEP_ANGLE:
        return SweepAngle();
    default:
        return ShapeAbstractItem::PropertyValue(property);
    }
}

void SectorItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        SetArcAngles(value.toDouble(), SweepAngle());
        break;
    case ItemProperty::SWEEP_ANGLE:
        SetArcAngles(StartAngle(), value.toDouble());
        break;
    default:
        ShapeAbstractItem::SetPropertyValue(property, value);
        break;
    }
}

ShapeItemState SectorItem::State() const
{
    ShapeItemState state = ShapeAbstractItem::State();
    state.propertyExtras.insert(ItemProperty::START_ANGLE, StartAngle());
    state.propertyExtras.insert(ItemProperty::SWEEP_ANGLE, SweepAngle());
    return state;
}

void SectorItem::ApplyState(const ShapeItemState& state)
{
    ShapeAbstractItem::ApplyState(state);
    const auto startIt = state.propertyExtras.constFind(ItemProperty::START_ANGLE);
    const auto sweepIt = state.propertyExtras.constFind(ItemProperty::SWEEP_ANGLE);
    if (startIt != state.propertyExtras.constEnd() || sweepIt != state.propertyExtras.constEnd()) {
        const qreal start = startIt != state.propertyExtras.constEnd() ? startIt.value().toDouble() : StartAngle();
        const qreal sweep = sweepIt != state.propertyExtras.constEnd() ? sweepIt.value().toDouble() : SweepAngle();
        SetArcAngles(start, sweep);
    }
}

void SectorItem::ArcAnglesChanged()
{
    update();
}

void SectorItem::WriteJsonExtra(QJsonObject& json) const
{
    WriteArcAngleJson(json);
}

void SectorItem::ReadJsonExtra(const QJsonObject& json)
{
    ReadArcAngleJson(json);
}

QPainterPath SectorItem::ShapePath(const QRectF& rect) const
{
    Q_UNUSED(rect)
    return Sector(StartAngle(), SweepAngle());
}
