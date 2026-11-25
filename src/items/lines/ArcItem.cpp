#include "items/lines/ArcItem.h"

#include <QPainter>
#include <QVector>

namespace {

constexpr int ARC_SAMPLES = 64;

} // namespace

ArcItem::ArcItem(QGraphicsItem* parent) : LineAbstractItem(parent) {}

ItemPropertySet ArcItem::Properties() const
{
    // 箭头行与其它描线子型一致地开放（弧线可设两端箭头）；长短半轴按真实椭圆外接框读写。
    return LineAbstractItem::Properties()
        .United(SectorBase::ArcAngleProperties())
        .United(SectorBase::ArcAxisProperties())
        .United(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END, ItemProperty::CENTER});
}

QPointF ArcItem::CenterScenePoint() const
{
    // 椭圆外接框在图元本地坐标里；先取本地框中心再映射，避免再叠一次 pos()。
    return mapToScene(FrameRect().center());
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
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
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
    case ItemProperty::ARROW_START:
        SetArrowStyles(static_cast<ArrowStyle>(value.toInt()), EndArrow());
        break;
    case ItemProperty::ARROW_END:
        SetArrowStyles(StartArrow(), static_cast<ArrowStyle>(value.toInt()));
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
    // 弧线本质是“椭圆上截一段轮廓”，选框必须是生成它的那个椭圆的外接框（§D12）；
    // 可见笔迹的包围盒会被归一化收缩，起止角一变框就跟着变，不能用。
    return EllipseFrameLocal(ContentSize());
}

QPointF ArcItem::RotationOrigin() const
{
    return FrameRect().center();
}

void ArcItem::SetAngles(qreal startDeg, qreal sweepDeg)
{
    SectorBase::SetArcAngles(startDeg, EllipseGeometry::ClampedSweep(sweepDeg));
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
    // 插入路径要给出确定的起点：直接写角度，避免在旧框上先重建一次。
    m_startAngle = startDeg;
    m_sweepAngle = EllipseGeometry::ClampedSweep(sweepDeg);

    const QRectF frameScene(topLeftScene, size);
    SetFrame(frameScene);
    SetStrokePoints(Sample(m_startAngle, m_sweepAngle, ARC_SAMPLES), color, width, 0.0, false);
    SetEllipseFrameScene(frameScene, ContentSize(), pos());
}

void ArcItem::SetArcAxes(qreal majorAxis, qreal minorAxis)
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF frameScene = EllipseFrameScene(box, pos());
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

HandleSpecList ArcItem::CustomHandles() const
{
    return ArcHandleSpecs(FrameRect());
}

bool ArcItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    SetAngleReadoutVisible(true);
    return true;
}

bool ArcItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    DragArcEndpoint(FrameRect(), mapFromScene(scenePos), handle == CanvasItem::CUSTOM_0);
    SetAngleReadoutVisible(true);
    return true;
}

void ArcItem::EndHandleDrag()
{
    SetAngleReadoutVisible(false);
}

void ArcItem::DrawSelectionOverlay(QPainter* painter)
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

QList<QRectF> ArcItem::AngleReadoutRects() const
{
    if (!AngleReadoutVisible()) {
        return {};
    }
    return {ReadoutRectAt(ArcReadoutAnchor(FrameRect(), ViewScale()), ArcReadoutText())};
}

qreal ArcItem::FramePadding() const
{
    return qMax<qreal>(42.0, ReadoutPadding());
}

void ArcItem::Rebuild()
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    RebuildWithFrame(EllipseFrameScene(box, pos()));
}

void ArcItem::RebuildWithFrame(const QRectF& frameScene)
{
    SetFrame(frameScene);
    SetStrokePoints(Sample(m_startAngle, m_sweepAngle, ARC_SAMPLES), StrokeColor(), StrokeWidth(), 0.0, false);
    SetEllipseFrameScene(frameScene, ContentSize(), pos());
    update();
}

QJsonObject ArcItem::ToJson() const
{
    QJsonObject json = LineAbstractItem::ToJson();
    WriteArcAngleJson(json);
    WriteArcFrameJson(json);
    return json;
}

void ArcItem::FromJson(const QJsonObject& json)
{
    LineAbstractItem::FromJson(json);
    ReadArcAngleJson(json);
    ReadArcFrameJson(json);
    Rebuild();
}
