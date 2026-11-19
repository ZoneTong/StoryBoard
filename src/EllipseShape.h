#pragma once

#include <QPointF>
#include <QPainterPath>
#include <QRectF>
#include <QVector>

// 椭圆 / 扇形 / 弧线共用的纯几何单元：非 QObject，不持有图元状态，可被组合（椭圆、扇形）
// 也可被继承（弧线图元），保证三种形态只有一份角度语义与扫过角兜底。
// 角度沿用 Qt 语义：0° 在三点钟方向、逆时针为正；外接框即内容矩形，长短半轴 = 宽高一半。
class EllipseShape {
  public:
    EllipseShape() = default; // 供同时继承曲线与椭圆的图元先默认构造、再由 setFrame 定位
    explicit EllipseShape(const QRectF& frame);

    QRectF frame() const { return m_frame; }
    void setFrame(const QRectF& frame) { m_frame = frame.normalized(); }
    QPointF center() const { return m_frame.center(); }
    qreal radiusX() const { return m_frame.width() / 2.0; }
    qreal radiusY() const { return m_frame.height() / 2.0; }

    // 整圆 / 整椭圆，闭合。
    QPainterPath ellipse() const;
    // 开放弧线，不闭合。
    QPainterPath arc(qreal startDeg, qreal sweepDeg) const;
    // 扇形：弧线两端到圆心闭合（闭合点就是 frame 中心）。
    QPainterPath sector(qreal startDeg, qreal sweepDeg) const;

    // 选框必须由生成路径的包围盒给出，解析近似会让扇形 90° 时差出几个像素。
    QRectF arcBounds(qreal startDeg, qreal sweepDeg) const;
    QRectF sectorBounds(qreal startDeg, qreal sweepDeg) const;

    // 按角度均匀采样（不是按弧长），供弧线图元转折线几何。
    QVector<QPointF> sample(qreal startDeg, qreal sweepDeg, int count) const;

    // 扫过角夹在 ±360°，0 兜底 120°：插入扇形 / 弧线后不能“什么都没有”。
    static qreal clampedSweep(qreal sweepDeg);

  private:
    QRectF m_frame;
};
