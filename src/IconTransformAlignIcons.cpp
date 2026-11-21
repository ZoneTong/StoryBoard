#include "IconTransformAlignIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

void DrawRotate(QPainter& painter, IconId id)
{
    const bool clockwise = id == IconId::ROTATE_CW;
    const QRectF circle(3.5, 3.5, 17.0, 17.0);
    painter.drawArc(circle, clockwise ? 40 * 16 : 140 * 16, clockwise ? 280 * 16 : -280 * 16);

    // Small solid arrowhead on the upper arc to show the rotation direction.
    painter.save();
    painter.setBrush(painter.pen().color());
    QPainterPath arrow;
    if (clockwise) {
        arrow.moveTo(15.5, 2.8);
        arrow.lineTo(20.0, 5.3);
        arrow.lineTo(16.6, 8.4);
    } else {
        arrow.moveTo(8.5, 2.8);
        arrow.lineTo(4.0, 5.3);
        arrow.lineTo(7.4, 8.4);
    }
    arrow.closeSubpath();
    painter.drawPath(arrow);
    painter.restore();
}

void DrawFit(QPainter& painter, IconId)
{
    const qreal m = 4.0;
    const qreal len = 5.0;
    painter.drawLine(QPointF(m, m + len), QPointF(m, m));
    painter.drawLine(QPointF(m, m), QPointF(m + len, m));
    painter.drawLine(QPointF(24.0 - m - len, m), QPointF(24.0 - m, m));
    painter.drawLine(QPointF(24.0 - m, m), QPointF(24.0 - m, m + len));
    painter.drawLine(QPointF(m, 24.0 - m - len), QPointF(m, 24.0 - m));
    painter.drawLine(QPointF(m, 24.0 - m), QPointF(m + len, 24.0 - m));
    painter.drawLine(QPointF(24.0 - m - len, 24.0 - m), QPointF(24.0 - m, 24.0 - m));
    painter.drawLine(QPointF(24.0 - m, 24.0 - m), QPointF(24.0 - m, 24.0 - m - len));
    painter.setBrush(painter.pen().color());
    painter.drawEllipse(QPointF(12.0, 12.0), 1.4, 1.4);
}

void DrawZoom(QPainter& painter, IconId id)
{
    painter.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
    painter.drawLine(QPointF(15.2, 15.2), QPointF(20.5, 20.5));
    painter.drawLine(QPointF(7.6, 10.5), QPointF(13.4, 10.5));
    if (id == IconId::ZOOM_IN) {
        painter.drawLine(QPointF(10.5, 7.6), QPointF(10.5, 13.4));
    }
}

void DrawAlign(QPainter& painter, IconId)
{
    // 三条左对齐的短线，表达“对齐 / 分布”。
    painter.setPen(QPen(painter.pen().color(), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(3.5, 5.0), QPointF(3.5, 19.0));
    painter.drawLine(QPointF(7.0, 7.5), QPointF(18.5, 7.5));
    painter.drawLine(QPointF(7.0, 12.0), QPointF(15.0, 12.0));
    painter.drawLine(QPointF(7.0, 16.5), QPointF(20.5, 16.5));
}

void DrawAlignMode(QPainter& painter, IconId id)
{
    // 一条基准线 + 两块被摆齐的实心块：基准线在哪一侧就是对齐到哪一侧。
    const QColor color = painter.pen().color();
    auto bar = [&painter, color](const QRectF& rect) { painter.fillRect(rect, color); };
    auto guideV = [&painter, color](qreal x) {
        painter.save();
        painter.setPen(QPen(color, 1.2, Qt::SolidLine));
        painter.drawLine(QPointF(x, 3.0), QPointF(x, 21.0));
        painter.restore();
    };
    auto guideH = [&painter, color](qreal y) {
        painter.save();
        painter.setPen(QPen(color, 1.2, Qt::SolidLine));
        painter.drawLine(QPointF(3.0, y), QPointF(21.0, y));
        painter.restore();
    };

    if (id == IconId::ALIGN_LEFT) {
        guideV(3.5);
        bar(QRectF(5.5, 6.0, 12.0, 4.0));
        bar(QRectF(5.5, 14.0, 8.0, 4.0));
    } else if (id == IconId::ALIGN_CENTER_H) {
        guideV(12.0);
        bar(QRectF(4.0, 6.0, 16.0, 4.0));
        bar(QRectF(7.5, 14.0, 9.0, 4.0));
    } else if (id == IconId::ALIGN_RIGHT) {
        guideV(20.5);
        bar(QRectF(6.5, 6.0, 12.0, 4.0));
        bar(QRectF(10.5, 14.0, 8.0, 4.0));
    } else if (id == IconId::ALIGN_TOP) {
        guideH(3.5);
        bar(QRectF(6.0, 5.5, 4.0, 12.0));
        bar(QRectF(14.0, 5.5, 4.0, 8.0));
    } else if (id == IconId::ALIGN_CENTER_V) {
        guideH(12.0);
        bar(QRectF(6.0, 4.0, 4.0, 16.0));
        bar(QRectF(14.0, 7.5, 4.0, 9.0));
    } else if (id == IconId::ALIGN_BOTTOM) {
        guideH(20.5);
        bar(QRectF(6.0, 6.5, 4.0, 12.0));
        bar(QRectF(14.0, 10.5, 4.0, 8.0));
    } else if (id == IconId::DISTRIBUTE_H) {
        bar(QRectF(4.0, 5.0, 4.0, 14.0));
        bar(QRectF(10.0, 5.0, 4.0, 14.0));
        bar(QRectF(16.0, 5.0, 4.0, 14.0));
    } else {
        bar(QRectF(5.0, 4.0, 14.0, 4.0));
        bar(QRectF(5.0, 10.0, 14.0, 4.0));
        bar(QRectF(5.0, 16.0, 14.0, 4.0));
    }
}

void DrawCopy(QPainter& painter, IconId)
{
    // 前后错位的两张圆角卡片，表示复制一份。
    const QColor color = painter.pen().color();
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(QRectF(3.5, 3.5, 11.5, 11.5), 2.0, 2.0);
    painter.drawRoundedRect(QRectF(9.0, 9.0, 11.5, 11.5), 2.0, 2.0);
    painter.restore();
}

void DrawGroup(QPainter& p, IconId id)
{
    // 虚线框住的两个小方块 = 组合；取消组合时虚线框只剩四角，方块也分开。
    QColor c = p.pen().color();
    p.save();
    p.setBrush(Qt::NoBrush);
    if (id == IconId::GROUP) {
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawRoundedRect(QRectF(2.5, 2.5, 19.0, 19.0), 2.5, 2.5);
    } else {
        // 需求 10：四角 L 形角框改为四个圆点（半径 1.9，线宽同族），
        // 与「组合」的虚线框明确区分。
    }
    if (id == IconId::UNGROUP) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal dotRadius = 1.7;
        for (const QPointF& center : {QPointF(3.0, 3.0), QPointF(21.0, 3.0), QPointF(3.0, 21.0), QPointF(21.0, 21.0)}) {
            p.drawEllipse(center, dotRadius, dotRadius);
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    if (id == IconId::GROUP) {
        p.drawRect(QRectF(6.0, 6.0, 5.0, 5.0));
        p.drawRect(QRectF(13.0, 13.0, 5.0, 5.0));
    } else {
        p.drawRect(QRectF(5.5, 5.5, 5.0, 5.0));
        p.drawRect(QRectF(13.5, 13.5, 5.0, 5.0));
    }
    p.restore();
}

void DrawMorph(QPainter& painter, IconId)
{
    // 圆 → 箭头 → 圆角方：表达“合并图元”，与并集 / 组合等五项子操作的图形语言区分开。
    const QColor color = painter.pen().color();
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawEllipse(QRectF(2.0, 7.0, 7.0, 7.0));
    painter.drawRoundedRect(QRectF(14.5, 6.5, 7.0, 7.0), 1.6, 1.6);
    painter.drawLine(QPointF(9.6, 10.5), QPointF(13.4, 10.5));
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    QPainterPath arrow;
    arrow.moveTo(12.6, 8.2);
    arrow.lineTo(15.2, 10.5);
    arrow.lineTo(12.6, 12.8);
    arrow.closeSubpath();
    painter.drawPath(arrow);
    painter.restore();
}


constexpr struct IconEntry {
    IconId id;
    void (*draw)(QPainter&, IconId);
} ENTRIES[] = {
    {IconId::ROTATE_CCW, DrawRotate},
    {IconId::ROTATE_CW, DrawRotate},
    {IconId::FIT, DrawFit},
    {IconId::ZOOM_IN, DrawZoom},
    {IconId::ZOOM_OUT, DrawZoom},
    {IconId::ALIGN, DrawAlign},
    {IconId::ALIGN_LEFT, DrawAlignMode},
    {IconId::ALIGN_CENTER_H, DrawAlignMode},
    {IconId::ALIGN_RIGHT, DrawAlignMode},
    {IconId::ALIGN_TOP, DrawAlignMode},
    {IconId::ALIGN_CENTER_V, DrawAlignMode},
    {IconId::ALIGN_BOTTOM, DrawAlignMode},
    {IconId::DISTRIBUTE_H, DrawAlignMode},
    {IconId::DISTRIBUTE_V, DrawAlignMode},
    {IconId::COPY, DrawCopy},
    {IconId::GROUP, DrawGroup},
    {IconId::UNGROUP, DrawGroup},
    {IconId::MORPH, DrawMorph},
};

} // namespace

bool PaintIconTransformAlign(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : ENTRIES) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}
