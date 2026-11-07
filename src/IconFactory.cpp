#include "IconFactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

void drawIconShape(QPainter& painter, IconId id)
{
    switch (id) {
    case IconId::New: {
        QPainterPath page;
        page.moveTo(5.0, 3.0);
        page.lineTo(14.0, 3.0);
        page.lineTo(18.0, 7.0);
        page.lineTo(18.0, 12.5);
        painter.drawPath(page);
        painter.drawLine(QPointF(14.0, 3.0), QPointF(14.0, 7.0));
        painter.drawLine(QPointF(14.0, 7.0), QPointF(18.0, 7.0));
        painter.drawLine(QPointF(5.0, 3.0), QPointF(5.0, 21.0));
        painter.drawLine(QPointF(5.0, 21.0), QPointF(13.0, 21.0));
        painter.setBrush(painter.pen().color());
        painter.drawEllipse(QPointF(18.0, 18.0), 4.5, 4.5);
        painter.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(18.0, 15.8), QPointF(18.0, 20.2));
        painter.drawLine(QPointF(15.8, 18.0), QPointF(20.2, 18.0));
        break;
    }
    case IconId::Open: {
        QPainterPath folder;
        folder.moveTo(3.0, 18.5);
        folder.lineTo(3.0, 6.0);
        folder.lineTo(9.0, 6.0);
        folder.lineTo(11.0, 8.5);
        folder.lineTo(19.0, 8.5);
        folder.lineTo(19.0, 10.5);
        painter.drawPath(folder);
        QPainterPath front;
        front.moveTo(3.0, 18.5);
        front.lineTo(6.0, 11.0);
        front.lineTo(22.0, 11.0);
        front.lineTo(19.0, 18.5);
        front.closeSubpath();
        painter.drawPath(front);
        break;
    }
    case IconId::Save: {
        painter.drawRoundedRect(QRectF(3.0, 3.0, 18.0, 18.0), 2.0, 2.0);
        painter.drawRect(QRectF(7.0, 3.0, 9.0, 6.0));
        painter.drawRoundedRect(QRectF(6.0, 13.0, 12.0, 7.0), 1.2, 1.2);
        painter.drawLine(QPointF(9.0, 3.0), QPointF(9.0, 6.5));
        break;
    }
    case IconId::Export: {
        painter.drawLine(QPointF(12.0, 3.0), QPointF(12.0, 14.0));
        painter.drawLine(QPointF(12.0, 3.0), QPointF(8.0, 7.0));
        painter.drawLine(QPointF(12.0, 3.0), QPointF(16.0, 7.0));
        QPainterPath tray;
        tray.moveTo(4.0, 13.0);
        tray.lineTo(4.0, 20.0);
        tray.lineTo(20.0, 20.0);
        tray.lineTo(20.0, 13.0);
        painter.drawPath(tray);
        break;
    }
    case IconId::Undo: {
        QPainterPath arc;
        arc.moveTo(19.0, 18.0);
        arc.cubicTo(20.0, 10.0, 13.0, 4.5, 5.5, 8.0);
        painter.drawPath(arc);
        painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 4.5));
        painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 11.5));
        break;
    }
    case IconId::Redo: {
        painter.save();
        painter.translate(24.0, 0.0);
        painter.scale(-1.0, 1.0);
        QPainterPath arc;
        arc.moveTo(19.0, 18.0);
        arc.cubicTo(20.0, 10.0, 13.0, 4.5, 5.5, 8.0);
        painter.drawPath(arc);
        painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 4.5));
        painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 11.5));
        painter.restore();
        break;
    }
    case IconId::View: {
        QPainterPath eye;
        eye.moveTo(2.5, 12.0);
        eye.cubicTo(7.0, 5.0, 17.0, 5.0, 21.5, 12.0);
        eye.cubicTo(17.0, 19.0, 7.0, 19.0, 2.5, 12.0);
        painter.drawPath(eye);
        painter.setBrush(painter.pen().color());
        painter.drawEllipse(QPointF(12.0, 12.0), 3.1, 3.1);
        break;
    }
    case IconId::Brush: {
        QPainterPath handle;
        handle.moveTo(10.0, 15.5);
        handle.lineTo(18.5, 6.0);
        handle.cubicTo(19.8, 4.6, 21.5, 6.5, 20.0, 7.8);
        handle.lineTo(11.8, 17.5);
        handle.closeSubpath();
        painter.drawPath(handle);
        QPainterPath tip;
        tip.moveTo(10.4, 16.6);
        tip.lineTo(8.0, 21.0);
        tip.lineTo(4.5, 17.8);
        tip.lineTo(9.0, 15.0);
        tip.closeSubpath();
        painter.drawPath(tip);
        break;
    }
    case IconId::Edit: {
        QPainterPath pointer;
        pointer.moveTo(5.5, 3.0);
        pointer.lineTo(5.5, 19.5);
        pointer.lineTo(9.8, 15.2);
        pointer.lineTo(13.0, 21.5);
        pointer.lineTo(16.0, 20.0);
        pointer.lineTo(12.8, 13.8);
        pointer.lineTo(18.5, 13.4);
        pointer.closeSubpath();
        painter.setBrush(Qt::white);
        painter.drawPath(pointer);
        break;
    }
    case IconId::Text: {
        painter.drawLine(QPointF(4.5, 5.0), QPointF(19.5, 5.0));
        painter.drawLine(QPointF(12.0, 5.0), QPointF(12.0, 20.0));
        painter.drawLine(QPointF(8.0, 20.0), QPointF(16.0, 20.0));
        painter.drawLine(QPointF(5.5, 8.0), QPointF(5.5, 5.0));
        painter.drawLine(QPointF(18.5, 8.0), QPointF(18.5, 5.0));
        break;
    }
    case IconId::Fit: {
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
        break;
    }
    case IconId::ZoomIn:
    case IconId::ZoomOut: {
        painter.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
        painter.drawLine(QPointF(15.2, 15.2), QPointF(20.5, 20.5));
        painter.drawLine(QPointF(7.6, 10.5), QPointF(13.4, 10.5));
        if (id == IconId::ZoomIn)
            painter.drawLine(QPointF(10.5, 7.6), QPointF(10.5, 13.4));
        break;
    }
    case IconId::Image: {
        painter.drawRoundedRect(QRectF(3.0, 4.0, 18.0, 16.0), 2.0, 2.0);
        painter.drawEllipse(QPointF(8.5, 9.0), 2.0, 2.0);
        QPainterPath hills;
        hills.moveTo(4.0, 18.0);
        hills.lineTo(9.5, 12.0);
        hills.lineTo(13.5, 16.0);
        hills.lineTo(16.5, 13.0);
        hills.lineTo(20.0, 18.0);
        painter.drawPath(hills);
        break;
    }
    case IconId::Background: {
        painter.drawRoundedRect(QRectF(3.0, 4.0, 18.0, 16.0), 2.0, 2.0);
        painter.save();
        painter.setClipRect(QRectF(4.0, 5.0, 16.0, 14.0));
        painter.fillRect(QRectF(4.0, 5.0, 5.0, 5.0), painter.pen().color());
        painter.fillRect(QRectF(14.0, 5.0, 5.0, 5.0), painter.pen().color());
        painter.fillRect(QRectF(9.0, 10.0, 5.0, 5.0), painter.pen().color());
        painter.fillRect(QRectF(4.0, 15.0, 5.0, 4.0), painter.pen().color());
        painter.fillRect(QRectF(19.0, 15.0, 1.5, 4.0), painter.pen().color());
        painter.restore();
        break;
    }
    case IconId::Settings: {
        painter.drawLine(QPointF(3.0, 6.5), QPointF(21.0, 6.5));
        painter.drawLine(QPointF(3.0, 12.0), QPointF(21.0, 12.0));
        painter.drawLine(QPointF(3.0, 17.5), QPointF(21.0, 17.5));
        painter.setBrush(Qt::white);
        painter.drawEllipse(QPointF(16.0, 6.5), 2.6, 2.6);
        painter.drawEllipse(QPointF(8.0, 12.0), 2.6, 2.6);
        painter.drawEllipse(QPointF(15.0, 17.5), 2.6, 2.6);
        break;
    }
    case IconId::Delete: {
        painter.drawLine(QPointF(4.0, 6.5), QPointF(20.0, 6.5));
        painter.drawLine(QPointF(9.5, 6.5), QPointF(9.5, 3.5));
        painter.drawLine(QPointF(9.5, 3.5), QPointF(14.5, 3.5));
        painter.drawLine(QPointF(14.5, 3.5), QPointF(14.5, 6.5));
        QPainterPath bin;
        bin.moveTo(6.0, 6.5);
        bin.lineTo(7.5, 21.0);
        bin.lineTo(16.5, 21.0);
        bin.lineTo(18.0, 6.5);
        painter.drawPath(bin);
        painter.drawLine(QPointF(10.0, 10.0), QPointF(10.5, 17.5));
        painter.drawLine(QPointF(14.0, 10.0), QPointF(13.5, 17.5));
        break;
    }
    case IconId::Grid: {
        painter.drawRoundedRect(QRectF(3.0, 3.0, 18.0, 18.0), 1.5, 1.5);
        painter.drawLine(QPointF(9.0, 3.0), QPointF(9.0, 21.0));
        painter.drawLine(QPointF(15.0, 3.0), QPointF(15.0, 21.0));
        painter.drawLine(QPointF(3.0, 9.0), QPointF(21.0, 9.0));
        painter.drawLine(QPointF(3.0, 15.0), QPointF(21.0, 15.0));
        break;
    }
    case IconId::Palette: {
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
        break;
    }
    case IconId::Folder: {
        QPainterPath folder;
        folder.moveTo(3.0, 19.0);
        folder.lineTo(3.0, 5.0);
        folder.lineTo(9.5, 5.0);
        folder.lineTo(11.5, 7.5);
        folder.lineTo(21.0, 7.5);
        folder.lineTo(21.0, 19.0);
        folder.closeSubpath();
        painter.drawPath(folder);
        painter.drawLine(QPointF(3.0, 10.0), QPointF(21.0, 10.0));
        break;
    }
    case IconId::Shapes: {
        painter.drawRoundedRect(QRectF(3.0, 3.0, 11.0, 9.0), 1.8, 1.8);
        painter.drawEllipse(QPointF(15.5, 15.0), 5.5, 5.5);
        QPainterPath triangle;
        triangle.moveTo(8.0, 21.0);
        triangle.lineTo(3.5, 13.5);
        triangle.lineTo(12.5, 13.5);
        triangle.closeSubpath();
        painter.drawPath(triangle);
        break;
    }
    }
}

} // namespace

QIcon makeIcon(IconId id, const QColor& color, int logicalSize)
{
    constexpr qreal iconGrid = 24.0;
    const qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.scale(logicalSize / iconGrid, logicalSize / iconGrid);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    drawIconShape(painter, id);
    painter.end();

    return QIcon(pixmap);
}

QIcon makeColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QRectF box(3.0, 3.0, 18.0, 18.0);
    painter.setPen(QPen(QColor(0, 0, 0, 50), 1.2));
    painter.setBrush(QBrush(QColor(255, 255, 255)));
    painter.drawRoundedRect(box, 5.0, 5.0);
    if (color.alpha() < 255) {
        painter.save();
        painter.setClipPath([&]() {
            QPainterPath path;
            path.addRoundedRect(box.adjusted(1.0, 1.0, -1.0, -1.0), 4.0, 4.0);
            return path;
        }());
        painter.fillRect(box.adjusted(1, 1, -1, -1), QColor(225, 228, 234));
        painter.fillRect(QRectF(3, 3, 9, 9), QColor(190, 195, 205));
        painter.fillRect(QRectF(12, 12, 9, 9), QColor(190, 195, 205));
        painter.restore();
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(box.adjusted(1.5, 1.5, -1.5, -1.5), 4.0, 4.0);

    return QIcon(pixmap);
}
