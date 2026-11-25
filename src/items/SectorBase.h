#pragma once

#include "items/HandleSpec.h"
#include "items/geometry/EllipseGeometry.h"
#include "items/properties/ItemProperties.h"

#include <QColor>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtGlobal>

// 扇形 / 弧线 / 角度线共用的椭圆截段状态：起止角、外接框比例、两个端点手柄、
// 开口读数的锚点与文本，以及 startAngle / sweepAngle / arcFrame 的序列化。
// “框 + 角度 → 路径 / 采样点 / 包围盒”的纯换算留在 EllipseGeometry；
// 本类不碰 QGraphicsItem 生命周期（不 update、不 mapFromScene），只提供数据与算法。
struct EllipseArcState {
    qreal startAngle = 30.0;
    qreal sweepAngle = 120.0;

    bool operator==(const EllipseArcState& other) const
    {
        return qFuzzyCompare(startAngle + 2.0, other.startAngle + 2.0) &&
               qFuzzyCompare(sweepAngle + 2.0, other.sweepAngle + 2.0);
    }
    bool operator!=(const EllipseArcState& other) const { return !(*this == other); }
};

class SectorBase : public EllipseGeometry {
  public:
    SectorBase() = default;
    ~SectorBase() = default;

    qreal StartAngle() const { return m_startAngle; }
    qreal SweepAngle() const { return m_sweepAngle; }
    // 角度统一夹在 ±360°；需要“0 扫过角兜底成 120°”的形态（弧线）在覆写里先做兜底。
    virtual void SetArcAngles(qreal startAngle, qreal sweepAngle);
    void SetStartAngle(qreal startAngle) { SetArcAngles(startAngle, m_sweepAngle); }
    void SetSweepAngle(qreal sweepAngle) { SetArcAngles(m_startAngle, sweepAngle); }

    // 外接框以“相对内容框的比例”为真相源：内容框拉伸后框跟着缩放，弧线改角度也不会跳。
    QRectF EllipseFrameRatio() const { return m_refFrame; }
    void SetEllipseFrameRatio(const QRectF& ratio) { m_refFrame = ratio.normalized(); }
    QRectF EllipseFrameLocal(const QSizeF& contentSize) const;
    // 外接框在图元场景坐标里的位置（本地框 + 图元原点），供描线采样使用。
    QRectF EllipseFrameScene(const QSizeF& contentSize, const QPointF& pos) const;
    // 真实椭圆外接尺寸（不是笔迹包围盒），供编辑面板显示长短半轴。
    QSizeF EllipseSizeFor(const QSizeF& contentSize) const;
    // 写入外接框（场景坐标）并回写比例；内容框变化后的同步也走这里。
    void SetEllipseFrameScene(const QRectF& frameScene, const QSizeF& contentSize, const QPointF& pos);
    // 框恒等于内容矩形的形态（扇形）用它把本地框贴回内容矩形。
    void SyncEllipseFrameFromRatio(const QSizeF& contentSize);

    // §3.3：两个端点手柄（CUSTOM_0 起始端、CUSTOM_1 结束端）与共享拖拽行为。
    HandleSpecList ArcHandleSpecs(const QRectF& frameLocal) const;
    void DragArcEndpoint(const QRectF& frameLocal, const QPointF& localPoint, bool startEndpoint);

    // 开口读数：锚点 + 文本 + 颜色在这里，绘制与字体由各图元按自己的读者尺寸完成。
    QPointF ArcReadoutAnchor(const QRectF& frameLocal, qreal viewScale) const;
    QString ArcReadoutText() const;
    static QColor ArcReadoutColor();

    static ItemPropertySet ArcAngleProperties();
    static ItemPropertySet ArcAxisProperties();
    // 长短半轴是否按“椭圆中心不动”的方式写入：弧线 / 角度线为 true，扇形沿用内容框缩放。
    virtual bool ArcAxisWritable() const { return false; }

    EllipseArcState ArcState() const;
    void ApplyArcState(const EllipseArcState& state);
    // 角度键：startAngle / sweepAngle，旧画布不需要迁移。
    void WriteArcAngleJson(QJsonObject& json) const;
    void ReadArcAngleJson(const QJsonObject& json);
    // 弧线 / 角度线的外接框比例键：arcFrame。扇形框恒等于内容框，不写这个键。
    void WriteArcFrameJson(QJsonObject& json) const;
    void ReadArcFrameJson(const QJsonObject& json);

  protected:
    // 角度变化后的刷新钩子：图形 update，弧线 / 角度线按新角度重建笔迹。
    virtual void ArcAnglesChanged() {}

    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
    QRectF m_refFrame{0.0, 0.0, 1.0, 1.0};
};
