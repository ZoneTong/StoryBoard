#include "items/shapes/SectorItem.h"

#include "items/geometry/EllipseAxis.h"

#include <QJsonObject>
#include <QPainter>
#include <cmath>

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
    // 扇形同时拥有弧角与椭圆半轴；自定义轮廓不再按椭圆外接框换算。
    ItemPropertySet properties = ShapeAbstractItem::Properties().United(ArcAngleAttributes::ArcAngleProperties());
    if (!IsCustomShape()) {
        properties.Insert(ItemProperty::MAJOR_AXIS);
        properties.Insert(ItemProperty::MINOR_AXIS);
    }
    return properties;
}

QVariant SectorItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        return StartAngle();
    case ItemProperty::SWEEP_ANGLE:
        return SweepAngle();
    case ItemProperty::MAJOR_AXIS:
        return EllipseAxis::Major(ContentSize());
    case ItemProperty::MINOR_AXIS:
        return EllipseAxis::Minor(ContentSize());
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
    case ItemProperty::MAJOR_AXIS:
    case ItemProperty::MINOR_AXIS: {
        const EllipseAxis::ResizeResult resize =
            EllipseAxis::Apply(property == ItemProperty::MAJOR_AXIS, value.toDouble(), pos(), ContentSize());
        setPos(resize.pos);
        SetContentSize(resize.size);
        break;
    }
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

HandleSpecList SectorItem::CustomHandles() const
{
    HandleSpecList specs;
    // 自定义轮廓没有弧角语义：不提供端点手柄。
    if (IsCustomShape()) {
        return specs;
    }
    const QRectF frame = Frame();
    HandleSpec start;
    start.id = CanvasItem::CUSTOM_0;
    start.position = EllipseShape::PointOnFrame(frame, StartAngle());
    start.behavior = HandleBehavior::ARC_ENDPOINT;
    start.cursor = Qt::SizeAllCursor;
    specs.append(start);
    HandleSpec end;
    end.id = CanvasItem::CUSTOM_1;
    end.position = EllipseShape::PointOnFrame(frame, StartAngle() + SweepAngle());
    end.behavior = HandleBehavior::ARC_ENDPOINT;
    end.cursor = Qt::SizeAllCursor;
    specs.append(end);
    return specs;
}

bool SectorItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    if (IsCustomShape() || (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1)) {
        return false;
    }
    SetAngleReadoutVisible(true);
    return true;
}

bool SectorItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (IsCustomShape() || (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1)) {
        return false;
    }
    DragArcEndpoint(Frame(), mapFromScene(scenePos), handle == CanvasItem::CUSTOM_0);
    SetAngleReadoutVisible(true);
    return true;
}

void SectorItem::EndHandleDrag()
{
    SetAngleReadoutVisible(false);
}

void SectorItem::DrawSelectionOverlay(QPainter* painter)
{
    ShapeAbstractItem::DrawSelectionOverlay(painter);
    if (!AngleReadoutVisible()) {
        return;
    }
    const QRectF frame = Frame();
    const QPointF point = EllipseShape::PointOnFrame(frame, StartAngle() + SweepAngle() / 2.0);
    QPointF radial = point - frame.center();
    const qreal length = std::hypot(radial.x(), radial.y());
    radial = length > 0.001 ? radial / length : QPointF(0.0, -1.0);
    const qreal offset = 18.0 / qMax<qreal>(ViewScale(), 0.001);
    const QPointF anchor = point + radial * offset;
    const QString text = QStringLiteral("%1°").arg(qRound(SweepAngle()));
    painter->save();
    painter->setFont(ReadoutFont(painter->font()));
    painter->setPen(QColor(37, 99, 235));
    painter->drawText(ReadoutRectAt(anchor, text), Qt::AlignCenter, text);
    painter->restore();
}

QList<QRectF> SectorItem::AngleReadoutRects() const
{
    if (!AngleReadoutVisible()) {
        return {};
    }
    const QRectF frame = Frame();
    const QPointF point = EllipseShape::PointOnFrame(frame, StartAngle() + SweepAngle() / 2.0);
    QPointF radial = point - frame.center();
    const qreal length = std::hypot(radial.x(), radial.y());
    radial = length > 0.001 ? radial / length : QPointF(0.0, -1.0);
    const qreal offset = 18.0 / qMax<qreal>(ViewScale(), 0.001);
    return {ReadoutRectAt(point + radial * offset, QStringLiteral("%1°").arg(qRound(SweepAngle())))};
}
