#include "items/SectorBase.h"

#include "items/CanvasItem.h"

#include <QJsonArray>
#include <QString>
#include <QtMath>
#include <cmath>

namespace {

// §3.3：端点拖到与另一端重合时至少留 1°，否则扇形 / 弧线会“什么都没有”。
constexpr qreal MIN_ENDPOINT_SWEEP = 1.0;
// 读数相对端点径向外推的距离（屏幕像素，按视图缩放折算到图元本地坐标）。
constexpr qreal READOUT_OFFSET = 18.0;

// 把角度差折到离参考值最近的同相位表示，保证连续拖拽不跳变（跨 ±180° 不翻转）。
qreal UnwrapNear(qreal value, qreal reference)
{
    while (value - reference > 180.0) {
        value -= 360.0;
    }
    while (value - reference < -180.0) {
        value += 360.0;
    }
    return value;
}

} // namespace

void SectorBase::SetArcAngles(qreal startAngle, qreal sweepAngle)
{
    const qreal start = qBound<qreal>(-360.0, startAngle, 360.0);
    const qreal sweep = qBound<qreal>(-360.0, sweepAngle, 360.0);
    if (qFuzzyCompare(m_startAngle + 2.0, start + 2.0) && qFuzzyCompare(m_sweepAngle + 2.0, sweep + 2.0)) {
        return;
    }
    m_startAngle = start;
    m_sweepAngle = sweep;
    ArcAnglesChanged();
}

QRectF SectorBase::EllipseFrameLocal(const QSizeF& contentSize) const
{
    return QRectF(m_refFrame.x() * contentSize.width(), m_refFrame.y() * contentSize.height(),
                  m_refFrame.width() * contentSize.width(), m_refFrame.height() * contentSize.height());
}

QRectF SectorBase::EllipseFrameScene(const QSizeF& contentSize, const QPointF& pos) const
{
    return EllipseFrameLocal(contentSize).translated(pos);
}

QSizeF SectorBase::EllipseSizeFor(const QSizeF& contentSize) const
{
    return QSizeF(qMax<qreal>(0.0, m_refFrame.width() * contentSize.width()),
                  qMax<qreal>(0.0, m_refFrame.height() * contentSize.height()));
}

void SectorBase::SetEllipseFrameScene(const QRectF& frameScene, const QSizeF& contentSize, const QPointF& pos)
{
    const QRectF frame = frameScene.normalized();
    SetFrame(frame);
    if (contentSize.width() > 0.0 && contentSize.height() > 0.0) {
        m_refFrame =
            QRectF((frame.left() - pos.x()) / contentSize.width(), (frame.top() - pos.y()) / contentSize.height(),
                   frame.width() / contentSize.width(), frame.height() / contentSize.height());
        return;
    }
    m_refFrame = QRectF(0.0, 0.0, 1.0, 1.0);
}

void SectorBase::SyncEllipseFrameFromRatio(const QSizeF& contentSize)
{
    SetFrame(EllipseFrameLocal(contentSize));
}

HandleSpecList SectorBase::ArcHandleSpecs(const QRectF& frameLocal) const
{
    HandleSpecList specs;
    if (frameLocal.width() <= 0.0 || frameLocal.height() <= 0.0) {
        return specs;
    }
    HandleSpec start;
    start.id = CanvasItem::CUSTOM_0;
    start.position = PointOnFrame(frameLocal, StartAngle());
    start.behavior = HandleBehavior::ARC_ENDPOINT;
    start.cursor = Qt::SizeAllCursor;
    specs.append(start);
    HandleSpec end;
    end.id = CanvasItem::CUSTOM_1;
    end.position = PointOnFrame(frameLocal, StartAngle() + SweepAngle());
    end.behavior = HandleBehavior::ARC_ENDPOINT;
    end.cursor = Qt::SizeAllCursor;
    specs.append(end);
    return specs;
}

void SectorBase::DragArcEndpoint(const QRectF& frameLocal, const QPointF& localPoint, bool startEndpoint)
{
    const QRectF frame = frameLocal.normalized();
    const qreal rx = frame.width() / 2.0;
    const qreal ry = frame.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return;
    }

    // 指针落入椭圆角度：Qt 语义 0° 在三点钟方向、逆时针为正，故 y 取负号。
    const QPointF center = frame.center();
    const qreal theta =
        qRound(qRadiansToDegrees(qAtan2((center.y() - localPoint.y()) / ry, (localPoint.x() - center.x()) / rx)));

    qreal newStart = m_startAngle;
    qreal newSweep = m_sweepAngle;
    if (startEndpoint) {
        // 拖起始端：结束端固定，只让 start 跟着指针走。
        const qreal endAngle = m_startAngle + m_sweepAngle;
        newStart = qRound(UnwrapNear(theta, m_startAngle));
        newSweep = qRound(UnwrapNear(endAngle - newStart, m_sweepAngle));
    } else {
        // 拖结束端：起始端固定，扫过角跟着指针走。
        newSweep = qRound(UnwrapNear(theta - m_startAngle, m_sweepAngle));
    }
    newSweep = qBound<qreal>(-360.0, newSweep, 360.0);
    if (qAbs(newSweep) < MIN_ENDPOINT_SWEEP) {
        newSweep = m_sweepAngle >= 0.0 ? MIN_ENDPOINT_SWEEP : -MIN_ENDPOINT_SWEEP;
    }
    SetArcAngles(newStart, newSweep);
}

QPointF SectorBase::ArcReadoutAnchor(const QRectF& frameLocal, qreal viewScale) const
{
    const QRectF frame = frameLocal.normalized();
    const QPointF point = PointOnFrame(frame, StartAngle() + SweepAngle() / 2.0);
    QPointF radial = point - frame.center();
    const qreal length = std::hypot(radial.x(), radial.y());
    radial = length > 0.001 ? radial / length : QPointF(0.0, -1.0);
    const qreal offset = READOUT_OFFSET / qMax<qreal>(viewScale, 0.001);
    return point + radial * offset;
}

QString SectorBase::ArcReadoutText() const
{
    return QStringLiteral("%1°").arg(qRound(SweepAngle()));
}

QColor SectorBase::ArcReadoutColor()
{
    return QColor(37, 99, 235);
}

ItemPropertySet SectorBase::ArcAngleProperties()
{
    return ItemPropertySet{ItemProperty::START_ANGLE, ItemProperty::SWEEP_ANGLE};
}

ItemPropertySet SectorBase::ArcAxisProperties()
{
    return ItemPropertySet{ItemProperty::MAJOR_AXIS, ItemProperty::MINOR_AXIS};
}

EllipseArcState SectorBase::ArcState() const
{
    return EllipseArcState{m_startAngle, m_sweepAngle};
}

void SectorBase::ApplyArcState(const EllipseArcState& state)
{
    SetArcAngles(state.startAngle, state.sweepAngle);
}

void SectorBase::WriteArcAngleJson(QJsonObject& json) const
{
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
}

void SectorBase::ReadArcAngleJson(const QJsonObject& json)
{
    SetArcAngles(json.value(QStringLiteral("startAngle")).toDouble(30.0),
                 json.value(QStringLiteral("sweepAngle")).toDouble(120.0));
}

void SectorBase::WriteArcFrameJson(QJsonObject& json) const
{
    QJsonArray array;
    array.append(m_refFrame.x());
    array.append(m_refFrame.y());
    array.append(m_refFrame.width());
    array.append(m_refFrame.height());
    json.insert(QStringLiteral("arcFrame"), array);
}

void SectorBase::ReadArcFrameJson(const QJsonObject& json)
{
    const QJsonArray array = json.value(QStringLiteral("arcFrame")).toArray();
    if (array.size() < 4) {
        m_refFrame = QRectF(0.0, 0.0, 1.0, 1.0);
        return;
    }
    m_refFrame = QRectF(array.at(0).toDouble(), array.at(1).toDouble(), array.at(2).toDouble(), array.at(3).toDouble());
}
