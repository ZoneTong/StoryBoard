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

ArcItem::ArcItem(QGraphicsItem* parent) : LineAbstractItem(parent) {}

ItemPropertySet ArcItem::Properties() const
{
    // 弧线只有角度参数，没有箭头行（与现状一致）；长短半轴按真实椭圆外接框读写。
    return LineAbstractItem::Properties()
        .United(ArcAngleAttributes::ArcAngleProperties())
        .United(ItemPropertySet{ItemProperty::MAJOR_AXIS, ItemProperty::MINOR_AXIS});
}

QVariant ArcItem::PropertyValue(ItemProperty property) const
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
    default:
        return LineAbstractItem::PropertyValue(property);
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
    case ItemProperty::MAJOR_AXIS:
        SetArcAxes(value.toDouble(), PropertyValue(ItemProperty::MINOR_AXIS).toDouble());
        break;
    case ItemProperty::MINOR_AXIS:
        SetArcAxes(PropertyValue(ItemProperty::MAJOR_AXIS).toDouble(), value.toDouble());
        break;
    default:
        LineAbstractItem::SetPropertyValue(property, value);
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

QSizeF ArcItem::EllipseSize() const
{
    const QSizeF box = ContentSize();
    return QSizeF(qMax<qreal>(0.0, m_refFrame.width() * box.width()),
                  qMax<qreal>(0.0, m_refFrame.height() * box.height()));
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

void ArcItem::SetArcAxes(qreal majorAxis, qreal minorAxis)
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_refFrame.x() * box.width(), m_refFrame.y() * box.height(), m_refFrame.width() * box.width(),
                       m_refFrame.height() * box.height());
    const QRectF frameScene = local.translated(pos());
    // 长轴沿外接框的宽或高：写入时保持另一根半轴不变，椭圆中心不动。
    const bool widthIsMajor = frameScene.width() >= frameScene.height();
    if (majorAxis < minorAxis) {
        minorAxis = majorAxis;
    }
    const qreal newWidth = widthIsMajor ? majorAxis * 2.0 : minorAxis * 2.0;
    const qreal newHeight = widthIsMajor ? minorAxis * 2.0 : majorAxis * 2.0;
    const QPointF center = frameScene.center();
    RebuildWithFrame(QRectF(center.x() - newWidth / 2.0, center.y() - newHeight / 2.0, newWidth, newHeight));
}

QRectF ArcItem::LocalEllipseFrame() const
{
    const QSizeF box = ContentSize();
    return QRectF(m_refFrame.x() * box.width(), m_refFrame.y() * box.height(), m_refFrame.width() * box.width(),
                  m_refFrame.height() * box.height());
}

HandleSpecList ArcItem::CustomHandles() const
{
    HandleSpecList specs;
    const QRectF frame = LocalEllipseFrame();
    if (frame.width() <= 0.0 || frame.height() <= 0.0) {
        return specs;
    }
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

bool ArcItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    return handle == CanvasItem::CUSTOM_0 || handle == CanvasItem::CUSTOM_1;
}

bool ArcItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    DragArcEndpoint(LocalEllipseFrame(), mapFromScene(scenePos), handle == CanvasItem::CUSTOM_0);
    return true;
}

void ArcItem::Rebuild()
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_refFrame.x() * box.width(), m_refFrame.y() * box.height(), m_refFrame.width() * box.width(),
                       m_refFrame.height() * box.height());
    RebuildWithFrame(local.translated(pos()));
}

void ArcItem::RebuildWithFrame(const QRectF& frameScene)
{
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
    QJsonObject json = LineAbstractItem::ToJson();
    WriteArcAngleJson(json);
    json.insert(QStringLiteral("arcFrame"), RectToArray(m_refFrame));
    return json;
}

void ArcItem::FromJson(const QJsonObject& json)
{
    LineAbstractItem::FromJson(json);
    ReadArcAngleJson(json);
    m_refFrame = ArrayToRect(json.value(QStringLiteral("arcFrame")).toArray(), QRectF(0.0, 0.0, 1.0, 1.0));
    Rebuild();
}
