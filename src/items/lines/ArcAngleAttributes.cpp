#include "items/lines/ArcAngleAttributes.h"

#include <QString>
#include <QtMath>

namespace {
// §3.3：端点拖到与另一端重合时至少留 1°，否则扇形 / 弧线会“什么都没有”。
constexpr qreal MIN_ENDPOINT_SWEEP = 1.0;

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

void ArcAngleAttributes::DragArcEndpoint(const QRectF& ellipseFrame, const QPointF& localPoint, bool startEndpoint)
{
    const QRectF frame = ellipseFrame.normalized();
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


void ArcAngleAttributes::SetArcAngles(qreal startAngle, qreal sweepAngle)
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

ArcAngleAttributesState ArcAngleAttributes::ArcAngleState() const
{
    return ArcAngleAttributesState{m_startAngle, m_sweepAngle};
}

void ArcAngleAttributes::ApplyArcAngleState(const ArcAngleAttributesState& state)
{
    SetArcAngles(state.startAngle, state.sweepAngle);
}

void ArcAngleAttributes::WriteArcAngleJson(QJsonObject& json) const
{
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
}

void ArcAngleAttributes::ReadArcAngleJson(const QJsonObject& json)
{
    SetArcAngles(json.value(QStringLiteral("startAngle")).toDouble(30.0),
                 json.value(QStringLiteral("sweepAngle")).toDouble(120.0));
}

ItemPropertySet ArcAngleAttributes::ArcAngleProperties()
{
    return ItemPropertySet{ItemProperty::START_ANGLE, ItemProperty::SWEEP_ANGLE};
}
