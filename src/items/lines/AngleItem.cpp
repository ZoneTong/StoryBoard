#include "items/lines/AngleItem.h"

#include <QPainter>
#include <QVector>
#include <QtMath>

namespace {

// 两条射线只由外端与顶点三点决定，采样密度不需要很高。
constexpr int ANGLE_POINTS = 3;

} // namespace

AngleItem::AngleItem(QGraphicsItem* parent) : LineAbstractItem(parent) {}

QString AngleItem::DisplayName() const
{
    return UiText(QStringLiteral("Angle line"));
}

QPointF AngleItem::CenterScenePoint() const
{
    // 椭圆外接框在图元本地坐标里；先取本地框中心（与 Qt 图元原点平移组合）再映射。
    return mapToScene(FrameRect().center());
}

QRectF AngleItem::FrameRect() const
{
    return EllipseFrameLocal(ContentSize());
}

QPointF AngleItem::RotationOrigin() const
{
    return FrameRect().center();
}

ItemPropertySet AngleItem::Properties() const
{
    return LineAbstractItem::Properties()
        .United(SectorBase::ArcAngleProperties())
        .United(SectorBase::ArcAxisProperties())
        .United(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END, ItemProperty::CENTER});
}

QVariant AngleItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        return StartAngle();
    case ItemProperty::SWEEP_ANGLE:
        return SweepAngle();
    case ItemProperty::MAJOR_AXIS:
        return qMax(EllipseSize().width(), EllipseSize().height()) / 2.0;
    case ItemProperty::MINOR_AXIS:
        return qMin(EllipseSize().width(), EllipseSize().height()) / 2.0;
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    default:
        return LineAbstractItem::PropertyValue(property);
    }
}

void AngleItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        SetAngles(value.toDouble(), SweepAngle());
        break;
    case ItemProperty::SWEEP_ANGLE:
        SetAngles(StartAngle(), value.toDouble());
        break;
    case ItemProperty::MAJOR_AXIS:
        SetArcAxes(value.toDouble(), PropertyValue(ItemProperty::MINOR_AXIS).toDouble());
        break;
    case ItemProperty::MINOR_AXIS:
        SetArcAxes(PropertyValue(ItemProperty::MAJOR_AXIS).toDouble(), value.toDouble());
        break;
    case ItemProperty::ARROW_START:
        LineAbstractItem::SetArrowStyles(static_cast<ArrowStyle>(value.toInt()), EndArrow());
        break;
    case ItemProperty::ARROW_END:
        LineAbstractItem::SetArrowStyles(StartArrow(), static_cast<ArrowStyle>(value.toInt()));
        break;
    default:
        LineAbstractItem::SetPropertyValue(property, value);
        break;
    }
}

void AngleItem::SetAngles(qreal startDeg, qreal sweepDeg)
{
    SectorBase::SetArcAngles(startDeg, sweepDeg);
}

void AngleItem::SetArcAngles(qreal startDeg, qreal sweepDeg)
{
    SetAngles(startDeg, sweepDeg);
}

void AngleItem::ArcAnglesChanged()
{
    Rebuild();
}

QVector<QPointF> AngleItem::AnglePoints(const QRectF& frameScene) const
{
    QVector<QPointF> points;
    const qreal rx = frameScene.width() / 2.0;
    const qreal ry = frameScene.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return points;
    }
    // 椭圆参数点：Qt 角度 0° 在三点钟方向、逆时针为正，故 y 分量取负。
    const QPointF center = frameScene.center();
    const qreal start = qDegreesToRadians(StartAngle());
    const qreal end = qDegreesToRadians(StartAngle() + SweepAngle());
    points.append(QPointF(center.x() + rx * std::cos(start), center.y() - ry * std::sin(start)));
    points.append(center);
    points.append(QPointF(center.x() + rx * std::cos(end), center.y() - ry * std::sin(end)));
    return points;
}

void AngleItem::SetAngleGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                                 const QColor& color, qreal width)
{
    m_startAngle = startDeg;
    m_sweepAngle = sweepDeg;
    const QRectF frameScene(topLeftScene, size);
    SetFrame(frameScene);
    const QVector<QPointF> points = AnglePoints(frameScene);
    if (points.size() != ANGLE_POINTS) {
        return;
    }
    // polyline=true：三点直连，顶点不会被 Catmull-Rom 磨圆，箭头方向才落在射线上。
    SetStrokePoints(points, color, width, 0.0, true);
    SetEllipseFrameScene(frameScene, ContentSize(), pos());
}

void AngleItem::SetArcAxes(qreal majorAxis, qreal minorAxis)
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF frameScene = EllipseFrameScene(box, pos());
    const bool widthIsMajor = frameScene.width() >= frameScene.height();
    if (majorAxis < minorAxis) {
        minorAxis = majorAxis;
    }
    const qreal newWidth = widthIsMajor ? majorAxis * 2.0 : minorAxis * 2.0;
    const qreal newHeight = widthIsMajor ? minorAxis * 2.0 : majorAxis * 2.0;
    const QPointF center = frameScene.center();
    RebuildWithFrame(QRectF(center.x() - newWidth / 2.0, center.y() - newHeight / 2.0, newWidth, newHeight));
}

HandleSpecList AngleItem::CustomHandles() const
{
    return ArcHandleSpecs(FrameRect());
}

bool AngleItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    SetAngleReadoutVisible(true);
    return true;
}

bool AngleItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    DragArcEndpoint(FrameRect(), mapFromScene(scenePos), handle == CanvasItem::CUSTOM_0);
    SetAngleReadoutVisible(true);
    return true;
}

void AngleItem::EndHandleDrag()
{
    SetAngleReadoutVisible(false);
}

void AngleItem::DrawSelectionOverlay(QPainter* painter)
{
    LineAbstractItem::DrawSelectionOverlay(painter);
    if (!AngleReadoutVisible()) {
        return;
    }
    const QString text = ArcReadoutText();
    painter->save();
    painter->setFont(ReadoutFont(painter->font()));
    painter->setPen(ArcReadoutColor());
    painter->drawText(ReadoutRectAt(ArcReadoutAnchor(FrameRect(), ViewScale()), text), Qt::AlignCenter, text);
    painter->restore();
}

QList<QRectF> AngleItem::AngleReadoutRects() const
{
    if (!AngleReadoutVisible()) {
        return {};
    }
    return {ReadoutRectAt(ArcReadoutAnchor(FrameRect(), ViewScale()), ArcReadoutText())};
}

qreal AngleItem::FramePadding() const
{
    return qMax<qreal>(42.0, ReadoutPadding());
}

void AngleItem::Rebuild()
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    RebuildWithFrame(EllipseFrameScene(box, pos()));
}

void AngleItem::RebuildWithFrame(const QRectF& frameScene)
{
    SetFrame(frameScene);
    const QVector<QPointF> points = AnglePoints(frameScene);
    if (points.size() != ANGLE_POINTS) {
        return;
    }
    SetStrokePoints(points, StrokeColor(), StrokeWidth(), 0.0, true);
    SetEllipseFrameScene(frameScene, ContentSize(), pos());
    update();
}

QJsonObject AngleItem::ToJson() const
{
    QJsonObject json = LineAbstractItem::ToJson();
    WriteArcAngleJson(json);
    WriteArcFrameJson(json);
    return json;
}

void AngleItem::FromJson(const QJsonObject& json)
{
    LineAbstractItem::FromJson(json);
    ReadArcAngleJson(json);
    ReadArcFrameJson(json);
    Rebuild();
}
