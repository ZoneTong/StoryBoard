#include "ArcItem.h"

#include <QJsonArray>
#include <QVector>

namespace {

constexpr qreal MINIMUM_FRAME_SIDE = 12.0;
constexpr int ARC_SAMPLES = 64;

QRectF paddedToMinimum(QRectF frame)
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

QJsonArray rectToArray(const QRectF& rect)
{
    QJsonArray array;
    array.append(rect.x());
    array.append(rect.y());
    array.append(rect.width());
    array.append(rect.height());
    return array;
}

QRectF arrayToRect(const QJsonArray& array, const QRectF& fallback)
{
    if (array.size() < 4) {
        return fallback;
    }
    return QRectF(array.at(0).toDouble(), array.at(1).toDouble(), array.at(2).toDouble(), array.at(3).toDouble());
}

} // namespace

ArcItem::ArcItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

ItemPropertySet ArcItem::properties() const
{
    // 弧线只有角度参数，没有箭头行（与现状一致）。
    return LineItemAbstract::properties().united(ItemPropertySet{ItemProperty::START_ANGLE, ItemProperty::SWEEP_ANGLE});
}

QVariant ArcItem::propertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        return m_startAngle;
    case ItemProperty::SWEEP_ANGLE:
        return m_sweepAngle;
    default:
        return LineItemAbstract::propertyValue(property);
    }
}

void ArcItem::setPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::START_ANGLE:
        setAngles(value.toDouble(), m_sweepAngle);
        break;
    case ItemProperty::SWEEP_ANGLE:
        setAngles(m_startAngle, value.toDouble());
        break;
    default:
        LineItemAbstract::setPropertyValue(property, value);
        break;
    }
}

QString ArcItem::displayName() const
{
    return uiText(QStringLiteral("Arc"));
}

QRectF ArcItem::frameRect() const
{
    return paddedToMinimum(CanvasItem::frameRect());
}

ArcItem::ParamState ArcItem::paramState() const
{
    return ParamState{m_startAngle, m_sweepAngle, true};
}

void ArcItem::applyParamState(const ParamState& state)
{
    setAngles(state.first, state.second);
}

void ArcItem::setAngles(qreal startDeg, qreal sweepDeg)
{
    const qreal sweep = EllipseShape::clampedSweep(sweepDeg);
    if (qFuzzyCompare(startDeg + 1.0, m_startAngle + 1.0) && qFuzzyCompare(sweep + 1.0, m_sweepAngle + 1.0)) {
        return;
    }
    m_startAngle = startDeg;
    m_sweepAngle = sweep;
    rebuild();
}

void ArcItem::setArcGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                             const QColor& color, qreal width)
{
    m_startAngle = startDeg;
    m_sweepAngle = EllipseShape::clampedSweep(sweepDeg);

    const QRectF frameScene(topLeftScene, size);
    setFrame(frameScene);
    setStrokePoints(sample(m_startAngle, m_sweepAngle, ARC_SAMPLES), color, width, 0.0, false);

    const QPointF origin = pos();
    const QSizeF box = contentSize();
    if (box.width() > 0.0 && box.height() > 0.0) {
        m_refFrame =
            QRectF((frameScene.left() - origin.x()) / box.width(), (frameScene.top() - origin.y()) / box.height(),
                   frameScene.width() / box.width(), frameScene.height() / box.height());
    } else {
        m_refFrame = QRectF(0.0, 0.0, 1.0, 1.0);
    }
}

void ArcItem::rebuild()
{
    const QSizeF box = contentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_refFrame.x() * box.width(), m_refFrame.y() * box.height(), m_refFrame.width() * box.width(),
                       m_refFrame.height() * box.height());
    const QRectF frameScene = local.translated(pos());
    setFrame(frameScene);
    setStrokePoints(sample(m_startAngle, m_sweepAngle, ARC_SAMPLES), strokeColor(), strokeWidth(), 0.0, false);

    const QPointF origin = pos();
    const QSizeF now = contentSize();
    if (now.width() > 0.0 && now.height() > 0.0) {
        m_refFrame =
            QRectF((frameScene.left() - origin.x()) / now.width(), (frameScene.top() - origin.y()) / now.height(),
                   frameScene.width() / now.width(), frameScene.height() / now.height());
    }
    update();
}

QJsonObject ArcItem::toJson() const
{
    QJsonObject json = LineItemAbstract::toJson();
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
    json.insert(QStringLiteral("arcFrame"), rectToArray(m_refFrame));
    return json;
}

void ArcItem::fromJson(const QJsonObject& json)
{
    LineItemAbstract::fromJson(json);
    m_startAngle = json.value(QStringLiteral("startAngle")).toDouble(30.0);
    m_sweepAngle = EllipseShape::clampedSweep(json.value(QStringLiteral("sweepAngle")).toDouble(120.0));
    m_refFrame = arrayToRect(json.value(QStringLiteral("arcFrame")).toArray(), QRectF(0.0, 0.0, 1.0, 1.0));
    rebuild();
}
