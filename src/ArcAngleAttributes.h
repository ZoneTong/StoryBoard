#pragma once

#include "ItemProperties.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>

// 起始角 / 扫过角属性基类：扇形（有面积图形）与弧线（零面积描线）共用这一份状态、
// 夹取、快照和 JSON 读写。几何仍由 EllipseShape 负责，本类不持有椭圆外接框，
// 避免把“角度属性”和“椭圆几何”混成同一个基类。
struct ArcAngleAttributesState {
    qreal startAngle = 30.0;
    qreal sweepAngle = 120.0;

    bool operator==(const ArcAngleAttributesState& other) const
    {
        return qFuzzyCompare(startAngle + 2.0, other.startAngle + 2.0) &&
               qFuzzyCompare(sweepAngle + 2.0, other.sweepAngle + 2.0);
    }
    bool operator!=(const ArcAngleAttributesState& other) const { return !(*this == other); }
};

class ArcAngleAttributes {
  public:
    ArcAngleAttributes() = default;
    virtual ~ArcAngleAttributes() = default;

    qreal StartAngle() const { return m_startAngle; }
    qreal SweepAngle() const { return m_sweepAngle; }

    // 角度统一限制在 ±360°；具体图元可在覆写里先做自己的兜底（弧线把 0 扫过角兜底成 120°）。
    virtual void SetArcAngles(qreal startAngle, qreal sweepAngle);
    void SetStartAngle(qreal startAngle) { SetArcAngles(startAngle, m_sweepAngle); }
    void SetSweepAngle(qreal sweepAngle) { SetArcAngles(m_startAngle, sweepAngle); }

    // §3.3 共享端点拖拽行为：拖起始端时结束端固定，拖结束端时起始端固定；
    // 只改 start / sweep（不改中心、半轴与外接框），扫过角限 ±360° 且最小 1°。
    // ellipseFrame 与 localPoint 都取图元本地坐标（旋转由调用方的 mapFromScene 消化）。
    void DragArcEndpoint(const QRectF& ellipseFrame, const QPointF& localPoint, bool startEndpoint);

    ArcAngleAttributesState ArcAngleState() const;
    void ApplyArcAngleState(const ArcAngleAttributesState& state);

    // JSON 键仍为 startAngle / sweepAngle，旧画布不需要迁移。
    void WriteArcAngleJson(QJsonObject& json) const;
    void ReadArcAngleJson(const QJsonObject& json);

    static ItemPropertySet ArcAngleProperties();

  protected:
    // 角度变化后的刷新钩子：图形 update，弧线按新角度重建描线几何。
    virtual void ArcAnglesChanged() {}

    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
};
