#include "ArcAngleAttributes.h"

#include <QString>

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
