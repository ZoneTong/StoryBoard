#include "IconTransformAlignIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

void drawRotate(QPainter& painter, IconId id)
{
    const bool clockwise = id == IconId::RotateCw;
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

void drawFit(QPainter& painter, IconId)
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

void drawZoom(QPainter& painter, IconId id)
{
    painter.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
    painter.drawLine(QPointF(15.2, 15.2), QPointF(20.5, 20.5));
    painter.drawLine(QPointF(7.6, 10.5), QPointF(13.4, 10.5));
    if (id == IconId::ZoomIn) {
        painter.drawLine(QPointF(10.5, 7.6), QPointF(10.5, 13.4));
    }
}

void drawAlign(QPainter& painter, IconId)
{
    // 三条左对齐的短线，表达“对齐 / 分布”。
    painter.setPen(QPen(painter.pen().color(), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(3.5, 5.0), QPointF(3.5, 19.0));
    painter.drawLine(QPointF(7.0, 7.5), QPointF(18.5, 7.5));
    painter.drawLine(QPointF(7.0, 12.0), QPointF(15.0, 12.0));
    painter.drawLine(QPointF(7.0, 16.5), QPointF(20.5, 16.5));
}

void drawAlignMode(QPainter& painter, IconId id)
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

    if (id == IconId::AlignLeft) {
        guideV(3.5);
        bar(QRectF(5.5, 6.0, 12.0, 4.0));
        bar(QRectF(5.5, 14.0, 8.0, 4.0));
    } else if (id == IconId::AlignCenterH) {
        guideV(12.0);
        bar(QRectF(4.0, 6.0, 16.0, 4.0));
        bar(QRectF(7.5, 14.0, 9.0, 4.0));
    } else if (id == IconId::AlignRight) {
        guideV(20.5);
        bar(QRectF(6.5, 6.0, 12.0, 4.0));
        bar(QRectF(10.5, 14.0, 8.0, 4.0));
    } else if (id == IconId::AlignTop) {
        guideH(3.5);
        bar(QRectF(6.0, 5.5, 4.0, 12.0));
        bar(QRectF(14.0, 5.5, 4.0, 8.0));
    } else if (id == IconId::AlignCenterV) {
        guideH(12.0);
        bar(QRectF(6.0, 4.0, 4.0, 16.0));
        bar(QRectF(14.0, 7.5, 4.0, 9.0));
    } else if (id == IconId::AlignBottom) {
        guideH(20.5);
        bar(QRectF(6.0, 6.5, 4.0, 12.0));
        bar(QRectF(14.0, 10.5, 4.0, 8.0));
    } else if (id == IconId::DistributeH) {
        bar(QRectF(4.0, 5.0, 4.0, 14.0));
        bar(QRectF(10.0, 5.0, 4.0, 14.0));
        bar(QRectF(16.0, 5.0, 4.0, 14.0));
    } else {
        bar(QRectF(5.0, 4.0, 14.0, 4.0));
        bar(QRectF(5.0, 10.0, 14.0, 4.0));
        bar(QRectF(5.0, 16.0, 14.0, 4.0));
    }
}

void drawCopy(QPainter& painter, IconId)
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

void drawGroup(QPainter& p, IconId id)
{
    // 虚线框住的两个小方块 = 组合；取消组合时虚线框只剩四角，方块也分开。
    QColor c = p.pen().color();
    p.save();
    p.setBrush(Qt::NoBrush);
    if (id == IconId::Group) {
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawRoundedRect(QRectF(2.5, 2.5, 19.0, 19.0), 2.5, 2.5);
    } else {
        // 需求 10：四角 L 形角框改为四个圆点（半径 1.9，线宽同族），
        // 与「组合」的虚线框明确区分。
    }
    if (id == IconId::Ungroup) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal dotRadius = 1.7;
        for (const QPointF& center : {QPointF(3.0, 3.0), QPointF(21.0, 3.0), QPointF(3.0, 21.0), QPointF(21.0, 21.0)}) {
            p.drawEllipse(center, dotRadius, dotRadius);
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    if (id == IconId::Group) {
        p.drawRect(QRectF(6.0, 6.0, 5.0, 5.0));
        p.drawRect(QRectF(13.0, 13.0, 5.0, 5.0));
    } else {
        p.drawRect(QRectF(5.5, 5.5, 5.0, 5.0));
        p.drawRect(QRectF(13.5, 13.5, 5.0, 5.0));
    }
    p.restore();
}

void drawMorph(QPainter& painter, IconId)
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
} kEntries[] = {
    {IconId::RotateCcw, drawRotate},      {IconId::RotateCw, drawRotate},        {IconId::Fit, drawFit},
    {IconId::ZoomIn, drawZoom},           {IconId::ZoomOut, drawZoom},           {IconId::Align, drawAlign},
    {IconId::AlignLeft, drawAlignMode},   {IconId::AlignCenterH, drawAlignMode}, {IconId::AlignRight, drawAlignMode},
    {IconId::AlignTop, drawAlignMode},    {IconId::AlignCenterV, drawAlignMode}, {IconId::AlignBottom, drawAlignMode},
    {IconId::DistributeH, drawAlignMode}, {IconId::DistributeV, drawAlignMode},  {IconId::Copy, drawCopy},
    {IconId::Group, drawGroup},           {IconId::Ungroup, drawGroup},          {IconId::Morph, drawMorph},
};

} // namespace

bool paintIconTransformAlign(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : kEntries) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}
