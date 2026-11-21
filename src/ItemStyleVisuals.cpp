#include "ItemStyleVisuals.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace {

QColor FillIconColor(const QColor& color)
{
    QColor fill = color.isValid() ? color : QColor(96, 132, 255);
    if (fill.alpha() < 150) {
        fill.setAlpha(150);
    }
    return fill;
}

} // namespace

namespace ItemStyleVisuals {

QList<Qt::PenStyle> BorderStyles()
{
    // 需求 15：描线样式头部新增“无”（Qt::NoPen，即完全透明）。
    return {Qt::NoPen, Qt::SolidLine, Qt::DashLine, Qt::DotLine, Qt::DashDotLine};
}

QString BorderStyleName(Qt::PenStyle style)
{
    switch (style) {
    case Qt::NoPen:
        return QCoreApplication::translate("ItemStyleVisuals", "None");
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

QIcon BorderStyleIcon(Qt::PenStyle style, int logicalSize)
{
    // 需求 14：四种描线样式图标共用一套生成器，图案按逻辑像素手调（以 16px 为基准等比缩放），
    // 不再依赖 Qt 笔宽倍数与圆头端点——那样 1.8px 的点会被糊成 3.6px 短划，点线与虚线难分。
    // 图标只是示意图，画布实际描线仍用 Qt::PenStyle。
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor color(58, 64, 80);
    const qreal k = logicalSize / 16.0;
    const qreal penWidth = (logicalSize >= 16 ? 2.0 : 1.6) * k;
    const qreal y = logicalSize / 2.0;
    const qreal x0 = 1.9 * k;
    const qreal x1 = logicalSize - 1.9 * k;
    const qreal dash = 4.5 * k;
    const qreal gap = 3.0 * k;
    const qreal dotRadius = penWidth * 0.55;

    QPen pen(color, penWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (style) {
    case Qt::NoPen: {
        // “无”样式：淡灰细线加一道斜杠，表示该图元不描线（透明）。
        painter.setPen(QPen(QColor(148, 163, 184), penWidth * 0.75, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawLine(QPointF(x0, y), QPointF(x1, y));
        painter.setPen(QPen(color, penWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawLine(QPointF(x0 + 1.0 * k, y + 2.4 * k), QPointF(x1 - 1.0 * k, y - 2.4 * k));
        break;
    }
    case Qt::DashLine: {
        qreal x = x0;
        while (x < x1) {
            painter.drawLine(QPointF(x, y), QPointF(qMin(x + dash, x1), y));
            x += dash + gap;
        }
        break;
    }
    case Qt::DotLine: {
        // 显式画圆点：16px 内约 4 个点，横向长度不超过 1.3 倍笔宽。
        qreal x = x0;
        painter.setBrush(color);
        while (x <= x1) {
            painter.drawEllipse(QPointF(x, y), dotRadius, dotRadius);
            x += gap + dotRadius * 2.0;
        }
        break;
    }
    case Qt::DashDotLine: {
        bool drawDash = true;
        qreal x = x0;
        while (x < x1) {
            if (drawDash) {
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(QPointF(x, y), QPointF(qMin(x + dash, x1), y));
                x += dash + gap;
            } else {
                painter.setBrush(color);
                if (x <= x1) {
                    painter.drawEllipse(QPointF(x + dotRadius, y), dotRadius, dotRadius);
                }
                x += dotRadius * 2.0 + gap;
            }
            drawDash = !drawDash;
        }
        break;
    }
    case Qt::SolidLine:
    default:
        painter.drawLine(QPointF(x0, y), QPointF(x1, y));
        break;
    }
    painter.end();
    return QIcon(pixmap);
}

QIcon FillColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor fill = FillIconColor(color);

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

QIcon BorderColorIcon(const QColor& color, int logicalSize)
{
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QColor ink(58, 64, 80);
    const QColor stroke = FillIconColor(color);
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

QIcon FillStyleIcon(FillStyle style, int logicalSize)
{
    return FillStyleIcon(style, QColor(96, 132, 255), logicalSize);
}

QIcon FillStyleIcon(FillStyle style, const QColor& color, int logicalSize)
{
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    const QRectF box(3.0, 4.0, 18.0, 16.0);
    const QColor ink(58, 64, 80);
    const QColor fill = FillIconColor(color);

    QPainterPath clipPath;
    clipPath.addRoundedRect(box.adjusted(1.0, 1.0, -1.0, -1.0), 2.5, 2.5);
    painter.save();
    painter.setClipPath(clipPath);

    if (style != FillStyle::NONE) {
        QColor background = fill;
        background.setAlpha(style == FillStyle::SOLID ? 255 : 70);
        painter.fillRect(box, background);
    }

    switch (style) {
    case FillStyle::NONE: {
        painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(5.5, 18.5), QPointF(18.5, 5.5));
        break;
    }
    case FillStyle::SOLID:
        break;
    case FillStyle::HACHURE:
    case FillStyle::REVERSE_HACHURE:
    case FillStyle::CROSS_HATCH: {
        painter.setPen(QPen(ink, 1.5));
        const bool forward = style != FillStyle::REVERSE_HACHURE;
        const bool backward = style != FillStyle::HACHURE;
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
    case FillStyle::GRID_HATCH: {
        painter.setPen(QPen(ink, 1.4));
        for (qreal x = box.left() + 4.0; x < box.right(); x += 4.5) {
            painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        }
        for (qreal y = box.top() + 4.0; y < box.bottom(); y += 4.5) {
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }
        break;
    }
    case FillStyle::HORIZONTAL_HATCH: {
        painter.setPen(QPen(ink, 1.4));
        for (qreal y = box.top() + 4.0; y < box.bottom(); y += 4.5) {
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }
        break;
    }
    case FillStyle::VERTICAL_HATCH: {
        painter.setPen(QPen(ink, 1.4));
        for (qreal x = box.left() + 4.0; x < box.right(); x += 4.5) {
            painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        }
        break;
    }
    case FillStyle::BRICK: {
        // 砖纹：三行横线，竖缝在行间交错。
        painter.setPen(QPen(ink, 1.4));
        const qreal rowHeight = box.height() / 3.0;
        for (int row = 0; row <= 3; ++row) {
            const qreal y = box.top() + row * rowHeight;
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
            if (row < 3) {
                const qreal offset = (row % 2) ? box.width() * 0.25 : box.width() * 0.5;
                const qreal nextY = y + rowHeight;
                painter.drawLine(QPointF(box.left() + offset, y), QPointF(box.left() + offset, nextY));
                painter.drawLine(QPointF(box.left() + offset + box.width() * 0.5, y),
                                 QPointF(box.left() + offset + box.width() * 0.5, nextY));
            }
        }
        break;
    }
    case FillStyle::DOTS: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 4; ++column) {
                const QPointF center(box.left() + 3.0 + column * 4.0 + (row % 2 ? 2.0 : 0.0),
                                     box.top() + 3.5 + row * 4.5);
                painter.drawEllipse(center, 1.1, 1.1);
            }
        }
        break;
    }
    case FillStyle::HONEYCOMB: {
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
