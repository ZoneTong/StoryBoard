#include "IconShapeGraphicIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

void drawShapes(QPainter& painter, IconId)
{
    painter.drawRoundedRect(QRectF(3.0, 3.0, 11.0, 9.0), 1.8, 1.8);
    painter.drawEllipse(QPointF(15.5, 15.0), 5.5, 5.5);
    QPainterPath triangle;
    triangle.moveTo(8.0, 21.0);
    triangle.lineTo(3.5, 13.5);
    triangle.lineTo(12.5, 13.5);
    triangle.closeSubpath();
    painter.drawPath(triangle);
}

void drawImage(QPainter& painter, IconId)
{
    painter.drawRoundedRect(QRectF(3.0, 4.0, 18.0, 16.0), 2.0, 2.0);
    painter.drawEllipse(QPointF(8.5, 9.0), 2.0, 2.0);
    QPainterPath hills;
    hills.moveTo(4.0, 18.0);
    hills.lineTo(9.5, 12.0);
    hills.lineTo(13.5, 16.0);
    hills.lineTo(16.5, 13.0);
    hills.lineTo(20.0, 18.0);
    painter.drawPath(hills);
}

void drawLineArt(QPainter& painter, IconId)
{
    // 一个画框加里面一条单线轮廓，表达“把图片描成线稿”。
    painter.drawRoundedRect(QRectF(3.0, 4.0, 18.0, 16.0), 2.0, 2.0);
    QPainterPath contour;
    contour.moveTo(5.5, 17.0);
    contour.lineTo(9.0, 10.5);
    contour.lineTo(12.5, 14.5);
    contour.lineTo(16.0, 8.5);
    contour.lineTo(18.5, 17.0);
    painter.drawPath(contour);
}

void drawBackground(QPainter& painter, IconId)
{
    painter.drawRoundedRect(QRectF(3.0, 4.0, 18.0, 16.0), 2.0, 2.0);
    painter.save();
    painter.setClipRect(QRectF(4.0, 5.0, 16.0, 14.0));
    painter.fillRect(QRectF(4.0, 5.0, 5.0, 5.0), painter.pen().color());
    painter.fillRect(QRectF(14.0, 5.0, 5.0, 5.0), painter.pen().color());
    painter.fillRect(QRectF(9.0, 10.0, 5.0, 5.0), painter.pen().color());
    painter.fillRect(QRectF(4.0, 15.0, 5.0, 4.0), painter.pen().color());
    painter.fillRect(QRectF(19.0, 15.0, 1.5, 4.0), painter.pen().color());
    painter.restore();
}

void drawGrid(QPainter& painter, IconId)
{
    painter.drawRoundedRect(QRectF(3.0, 3.0, 18.0, 18.0), 1.5, 1.5);
    painter.drawLine(QPointF(9.0, 3.0), QPointF(9.0, 21.0));
    painter.drawLine(QPointF(15.0, 3.0), QPointF(15.0, 21.0));
    painter.drawLine(QPointF(3.0, 9.0), QPointF(21.0, 9.0));
    painter.drawLine(QPointF(3.0, 15.0), QPointF(21.0, 15.0));
}

void drawPalette(QPainter& painter, IconId)
{
    QPainterPath palette;
    palette.moveTo(12.0, 3.0);
    palette.cubicTo(5.0, 3.0, 2.0, 9.0, 3.2, 14.5);
    palette.cubicTo(4.5, 20.5, 10.0, 21.5, 12.5, 19.0);
    palette.cubicTo(14.0, 17.5, 12.0, 16.0, 13.5, 14.5);
    palette.cubicTo(15.0, 13.0, 21.5, 15.0, 21.0, 9.5);
    palette.cubicTo(20.6, 5.5, 16.5, 3.0, 12.0, 3.0);
    painter.drawPath(palette);
    painter.setBrush(painter.pen().color());
    painter.drawEllipse(QPointF(8.5, 8.5), 1.4, 1.4);
    painter.drawEllipse(QPointF(13.0, 7.0), 1.4, 1.4);
    painter.drawEllipse(QPointF(7.5, 13.0), 1.4, 1.4);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(16.0, 12.0), 1.8, 1.8);
}

void drawCorner(QPainter& painter, IconId)
{
    // 一角带圆弧的方框，表达“矩形圆角”。
    QPainterPath frame;
    frame.moveTo(4.0, 16.5);
    frame.lineTo(4.0, 10.0);
    frame.quadTo(4.0, 4.0, 10.0, 4.0);
    frame.lineTo(20.0, 4.0);
    painter.drawPath(frame);
    painter.setBrush(painter.pen().color());
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(6.8, 6.8), 1.4, 1.4);
}

void drawSmooth(QPainter& painter, IconId)
{
    // 一条手绘波浪线加两个控制点，表达“把抖动收敛成平滑曲线”。
    QPainterPath wave;
    wave.moveTo(2.5, 16.0);
    wave.cubicTo(6.0, 4.0, 10.5, 21.0, 14.5, 10.0);
    wave.cubicTo(17.5, 3.0, 20.0, 12.0, 21.5, 16.0);
    painter.drawPath(wave);
    painter.setBrush(painter.pen().color());
    painter.drawEllipse(QPointF(2.5, 16.0), 1.5, 1.5);
    painter.drawEllipse(QPointF(21.5, 16.0), 1.5, 1.5);
    painter.setBrush(Qt::NoBrush);
}

void drawOpacity(QPainter& painter, IconId)
{
    // 左实右虚的圆，表达“不透明度”。
    QPainterPath left;
    left.moveTo(12.0, 3.5);
    left.arcTo(QRectF(3.5, 3.5, 17.0, 17.0), 90.0, 180.0);
    left.closeSubpath();
    painter.setBrush(painter.pen().color());
    painter.drawPath(left);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(painter.pen().color(), 1.9, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawArc(QRectF(3.5, 3.5, 17.0, 17.0), -90 * 16, 180 * 16);
}

void drawTriangleBase(QPainter& painter, IconId)
{
    // 只有底线与两条向内收的短斜边，不闭合顶点，读作“三角形底边”。
    painter.drawLine(QPointF(3.0, 18.5), QPointF(21.0, 18.5));
    painter.drawLine(QPointF(3.0, 18.5), QPointF(8.0, 9.5));
    painter.drawLine(QPointF(21.0, 18.5), QPointF(16.0, 9.5));
}

void drawAngleRight(QPainter& painter, IconId)
{
    // 底角 A：顶点在左下，一条边沿底边向右、另一条斜向右上，开口朝右。
    painter.drawLine(QPointF(4.0, 18.0), QPointF(20.0, 18.0));
    painter.drawLine(QPointF(4.0, 18.0), QPointF(18.5, 6.0));
}

void drawAngleLeft(QPainter& painter, IconId)
{
    // 底角 B：顶点在右下，一条边沿底边向左、另一条斜向左上，开口朝左。
    painter.drawLine(QPointF(20.0, 18.0), QPointF(4.0, 18.0));
    painter.drawLine(QPointF(20.0, 18.0), QPointF(5.5, 6.0));
}

constexpr struct IconEntry {
    IconId id;
    void (*draw)(QPainter&, IconId);
} kEntries[] = {
    {IconId::Shapes, drawShapes},
    {IconId::Image, drawImage},
    {IconId::LineArt, drawLineArt},
    {IconId::Background, drawBackground},
    {IconId::Grid, drawGrid},
    {IconId::Palette, drawPalette},
    {IconId::Corner, drawCorner},
    {IconId::Smooth, drawSmooth},
    {IconId::Opacity, drawOpacity},
    {IconId::TriangleBase, drawTriangleBase},
    {IconId::AngleRight, drawAngleRight},
    {IconId::AngleLeft, drawAngleLeft},
};

} // namespace

bool paintIconShapeGraphic(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : kEntries) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}
