#include "ui/icons/IconLayerViewIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace IconPainting {

namespace {

void DrawView(QPainter& painter, IconId)
{
    QPainterPath eye;
    eye.moveTo(2.5, 12.0);
    eye.cubicTo(7.0, 5.0, 17.0, 5.0, 21.5, 12.0);
    eye.cubicTo(17.0, 19.0, 7.0, 19.0, 2.5, 12.0);
    painter.drawPath(eye);
    painter.setBrush(painter.pen().color());
    painter.drawEllipse(QPointF(12.0, 12.0), 3.1, 3.1);
}

void DrawLock(QPainter& painter, IconId)
{
    QPainterPath shackle;
    shackle.moveTo(8.0, 12.5);
    shackle.lineTo(8.0, 9.0);
    shackle.cubicTo(8.0, 3.8, 16.0, 3.8, 16.0, 9.0);
    shackle.lineTo(16.0, 12.5);
    painter.drawPath(shackle);
    const QColor lockColor = painter.pen().color();
    painter.setBrush(lockColor);
    painter.drawRoundedRect(QRectF(4.0, 12.5, 16.0, 9.0), 2.2, 2.2);
    painter.setBrush(QColor(255, 255, 255));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(12.0, 16.0), 1.6, 1.6);
    painter.drawRect(QRectF(11.2, 16.0, 1.6, 3.2));
    painter.setPen(QPen(lockColor, 1.6));
    painter.setBrush(Qt::NoBrush);
}

void DrawLayerUp(QPainter& painter, IconId)
{
    // 双层页片整体上移，表达“跨图层移动”。
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(5.0, 7.0, 12.0, 15.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(3.0, 5.0, 12.0, 15.0));
    painter.setBrush(painter.pen().color());
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRectF(3.0, 5.0, 12.0, 15.0));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(painter.pen().color(), 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(3.0, 5.0, 12.0, 15.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(18.5, 14.5), QPointF(18.5, 21.5));
    painter.drawLine(QPointF(18.5, 14.5), QPointF(15.8, 17.2));
    painter.drawLine(QPointF(18.5, 14.5), QPointF(21.2, 17.2));
}

void DrawLayerDown(QPainter& painter, IconId)
{
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(5.0, 2.0, 12.0, 15.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(3.0, 4.0, 12.0, 15.0));
    painter.setBrush(painter.pen().color());
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRectF(3.0, 4.0, 12.0, 15.0));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(painter.pen().color(), 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(3.0, 4.0, 12.0, 15.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(18.5, 9.5), QPointF(18.5, 16.5));
    painter.drawLine(QPointF(18.5, 16.5), QPointF(15.8, 13.8));
    painter.drawLine(QPointF(18.5, 16.5), QPointF(21.2, 13.8));
}

void DrawOrderUp(QPainter& painter, IconId)
{
    // 方块上移一层，表达“层内顺序 +1”。
    painter.setBrush(painter.pen().color());
    painter.drawRect(QRectF(4.0, 12.0, 10.0, 9.0));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(painter.pen().color(), 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(10.0, 3.0, 10.0, 9.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(15.5, 15.5), QPointF(15.5, 22.5));
    painter.drawLine(QPointF(15.5, 15.5), QPointF(12.8, 18.2));
    painter.drawLine(QPointF(15.5, 15.5), QPointF(18.2, 18.2));
}

void DrawOrderDown(QPainter& painter, IconId)
{
    painter.setBrush(painter.pen().color());
    painter.drawRect(QRectF(4.0, 3.0, 10.0, 9.0));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(painter.pen().color(), 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(QRectF(10.0, 12.0, 10.0, 9.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(15.5, 8.5), QPointF(15.5, 15.5));
    painter.drawLine(QPointF(15.5, 15.5), QPointF(12.8, 12.8));
    painter.drawLine(QPointF(15.5, 15.5), QPointF(18.2, 12.8));
}

constexpr struct IconEntry {
    IconId id;
    void (*draw)(QPainter&, IconId);
} ENTRIES[] = {
    {IconId::VIEW, DrawView},        {IconId::LOCK, DrawLock},
    {IconId::LAYER_UP, DrawLayerUp}, {IconId::LAYER_DOWN, DrawLayerDown},
    {IconId::ORDER_UP, DrawOrderUp}, {IconId::ORDER_DOWN, DrawOrderDown},
};

} // namespace

bool PaintLayerView(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : ENTRIES) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}

} // namespace IconPainting
