#include "ItemStyleVisuals.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace {

QColor fillIconColor(const QColor& color)
{
    QColor fill = color.isValid() ? color : QColor(96, 132, 255);
    if (fill.alpha() < 150) {
        fill.setAlpha(150);
    }
    return fill;
}

} // namespace

namespace ItemStyleVisuals {

QList<Qt::PenStyle> borderStyles()
{
    return {Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
}

QString borderStyleName(Qt::PenStyle style)
{
    switch (style) {
    case Qt::SolidLine:
        return QCoreApplication::translate("ItemStyleVisuals", "Solid");
    case Qt::DashLine:
        return QCoreApplication::translate("ItemStyleVisuals", "Dashed");
    case Qt::DotLine:
        return QCoreApplication::translate("ItemStyleVisuals", "Dotted");
    case Qt::DashDotLine:
        return QCoreApplication::translate("ItemStyleVisuals", "Dash-dot");
    default:
        return QCoreApplication::translate("ItemStyleVisuals", "Solid");
    }
}

QIcon borderStyleIcon(Qt::PenStyle style, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(58, 64, 80), 1.8, style, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(4.0, logicalSize / 2.0), QPointF(logicalSize - 4.0, logicalSize / 2.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon fillColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor fill = fillIconColor(color);

    QPainterPath bucket;
    bucket.moveTo(5.0, 9.0);
    bucket.lineTo(8.0, 20.0);
    bucket.lineTo(17.0, 20.0);
    bucket.lineTo(20.0, 9.0);
    bucket.closeSubpath();

    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(fill);
    painter.drawPath(bucket);

    QPainterPath handle;
    handle.moveTo(7.0, 9.0);
    handle.cubicTo(7.0, 2.5, 18.0, 2.5, 18.0, 9.0);
    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(handle);

    painter.setPen(QPen(ink, 1.9));
    painter.drawLine(QPointF(4.0, 9.0), QPointF(21.0, 9.0));
    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
    painter.drawLine(QPointF(9.5, 12.5), QPointF(11.0, 18.0));
    painter.end();

    return QIcon(pixmap);
}

QIcon borderColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor stroke = fillIconColor(color);
    const QRectF box(4.0, 4.0, 16.0, 16.0);

    painter.setPen(QPen(ink, 1.0));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.setPen(QPen(stroke, 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box.adjusted(2.0, 2.0, -2.0, -2.0), 2.2, 2.2);

    QPainterPath nib;
    nib.moveTo(14.5, 1.0);
    nib.lineTo(22.5, 9.0);
    nib.lineTo(18.5, 10.5);
    nib.lineTo(13.0, 5.0);
    nib.closeSubpath();
    painter.setPen(QPen(ink, 0.8));
    painter.setBrush(ink);
    painter.drawPath(nib);
    painter.end();

    return QIcon(pixmap);
}

QIcon fillStyleIcon(FillStyle style, int logicalSize)
{
    return fillStyleIcon(style, QColor(96, 132, 255), logicalSize);
}

QIcon fillStyleIcon(FillStyle style, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QRectF box(3.0, 4.0, 18.0, 16.0);
    const QColor ink(58, 64, 80);
    const QColor fill = fillIconColor(color);

    QPainterPath clipPath;
    clipPath.addRoundedRect(box.adjusted(1.0, 1.0, -1.0, -1.0), 2.5, 2.5);
    painter.save();
    painter.setClipPath(clipPath);

    if (style != FillStyle::None) {
        QColor background = fill;
        background.setAlpha(style == FillStyle::Solid ? 255 : 70);
        painter.fillRect(box, background);
    }

    switch (style) {
    case FillStyle::None: {
        painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(5.5, 18.5), QPointF(18.5, 5.5));
        break;
    }
    case FillStyle::Solid:
        break;
    case FillStyle::Hachure:
    case FillStyle::ReverseHachure:
    case FillStyle::CrossHatch: {
        painter.setPen(QPen(ink, 1.5));
        const bool forward = style != FillStyle::ReverseHachure;
        const bool backward = style != FillStyle::Hachure;
        for (qreal offset = -box.height(); offset < box.width() + box.height(); offset += 4.2) {
            if (forward) {
                painter.drawLine(QPointF(box.left() + offset, box.bottom()),
                                 QPointF(box.left() + offset + box.height(), box.top()));
            }
            if (backward) {
                painter.drawLine(QPointF(box.left() + offset, box.top()),
                                 QPointF(box.left() + offset + box.height(), box.bottom()));
            }
        }
        break;
    }
    case FillStyle::GridHatch: {
        painter.setPen(QPen(ink, 1.4));
        for (qreal x = box.left() + 4.0; x < box.right(); x += 4.5) {
            painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        }
        for (qreal y = box.top() + 4.0; y < box.bottom(); y += 4.5) {
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }
        break;
    }
    case FillStyle::Honeycomb: {
        painter.setPen(QPen(ink, 1.2));
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const QPointF center(box.left() + 4.0 + column * 5.5 + (row % 2 ? 2.75 : 0.0),
                                     box.top() + 3.5 + row * 5.0);
                QPolygonF hexagon;
                for (int i = 0; i < 6; ++i) {
                    const qreal angle = qDegreesToRadians(60.0 * i - 30.0);
                    hexagon << center + QPointF(2.8 * qCos(angle), 2.8 * qSin(angle));
                }
                painter.drawPolygon(hexagon);
            }
        }
        break;
    }
    }

    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(ink, 1.8));
    painter.drawRoundedRect(box, 3.0, 3.0);
    painter.end();

    return QIcon(pixmap);
}

} // namespace ItemStyleVisuals
