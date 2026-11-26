#include "items/lines/StraightLineItem.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"

#include <QJsonObject>
#include <QTransform>
#include <QVector>
#include <QtMath>

namespace {

constexpr qreal MINIMUM_FRAME_SIDE = 12.0;

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

} // namespace

StraightLineItem::StraightLineItem(QGraphicsItem* parent) : LineAbstractItem(parent) {}

ItemPropertySet StraightLineItem::Properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineAbstractItem::Properties().United(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant StraightLineItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    default:
        return LineAbstractItem::PropertyValue(property);
    }
}

void StraightLineItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
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

QString StraightLineItem::DisplayName() const
{
    return ArrowStyleIsHead(StartArrow()) || ArrowStyleIsHead(EndArrow()) ? UiText(QStringLiteral("Line Arrow"))
                                                                          : UiText(QStringLiteral("Straight Line"));
}

QRectF StraightLineItem::FrameRect() const
{
    return PaddedToMinimum(CanvasItem::FrameRect());
}

qreal StraightLineItem::DisplayRotationAngle() const
{
    const QVector<QPointF> knots = StrokePoints();
    if (knots.size() < 2) {
        return RotationAngle();
    }
    // 直线几何只由端点决定，旋转角只是叠加在绘制矩阵上的偏移；两者相加才是
    // 用户在画布上看到的走向。镜像只改显示矩阵，这里同样先把端点过一遍。
    QPointF start = knots.constFirst();
    QPointF end = knots.constLast();
    if (FlipX() || FlipY()) {
        const QTransform mirror = MirrorTransform();
        start = mirror.map(start);
        end = mirror.map(end);
    }
    const qreal direction = qRadiansToDegrees(qAtan2(end.y() - start.y(), end.x() - start.x()));
    return CanvasItem::NormalizeRotation(RotationAngle() + direction);
}

void StraightLineItem::SetDisplayRotationAngle(qreal angle)
{
    // 端点几何不动，只补足旋转角的差值：面板填 45° 时，直线正好转到 45°。
    const qreal delta = CanvasItem::NormalizeRotation(angle - DisplayRotationAngle());
    if (qAbs(delta) < 1e-6) {
        return;
    }
    SetRotationAngle(CanvasItem::NormalizeRotation(RotationAngle() + delta));
}

void StraightLineItem::SetEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color,
                                    qreal width)
{
    SetStrokePoints(QVector<QPointF>{startScene, endScene}, color, width, 0.0, true);
}

void StraightLineItem::SetLineRoute(RouteMode route)
{
    if (m_route == route) {
        return;
    }
    m_route = route;
    update();
}

void StraightLineItem::ApplyRouteGeometry(const QVector<QPointF>& scenePoints)
{
    if (scenePoints.size() < 2) {
        return;
    }
    // 折线按场景横纵轴排布，不叠加图元自身的旋转；否则水平/垂直段会被转成斜线。
    if (!qFuzzyIsNull(RotationAngle())) {
        SetRotationAngle(0.0);
    }
    SetStrokePoints(scenePoints, StrokeColor(), StrokeWidth(), 0.0, true);
}

QJsonObject StraightLineItem::ToJson() const
{
    QJsonObject json = LineAbstractItem::ToJson();
    json.insert(QStringLiteral("route"),
                m_route == RouteMode::ORTHOGONAL ? QStringLiteral("orthogonal") : QStringLiteral("direct"));
    return json;
}

void StraightLineItem::FromJson(const QJsonObject& json)
{
    LineAbstractItem::FromJson(json);
    m_route = json.value(QStringLiteral("route")).toString() == QStringLiteral("orthogonal") ? RouteMode::ORTHOGONAL
                                                                                             : RouteMode::DIRECT;
}

HandleSpecList StraightLineItem::CustomHandles() const
{
    HandleSpecList specs;
    const QVector<QPointF> knots = StrokePoints();
    if (knots.size() < 2) {
        return specs;
    }
    HandleSpec start;
    start.id = CanvasItem::CUSTOM_0;
    start.position = knots.constFirst();
    start.behavior = HandleBehavior::ARC_ENDPOINT;
    start.cursor = Qt::SizeAllCursor;
    start.sizeScale = 0.9;
    specs.append(start);
    HandleSpec end;
    end.id = CanvasItem::CUSTOM_1;
    end.position = knots.constLast();
    end.behavior = HandleBehavior::ARC_ENDPOINT;
    end.cursor = Qt::SizeAllCursor;
    end.sizeScale = 0.9;
    specs.append(end);
    return specs;
}

bool StraightLineItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    return handle == CanvasItem::CUSTOM_0 || handle == CanvasItem::CUSTOM_1;
}

bool StraightLineItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    // 吸附 / 解绑口径与曲线共用：落点交给 LineAbstractItem 统一处理。
    return DragEndpointWithBinding(handle == CanvasItem::CUSTOM_0, scenePos);
}
