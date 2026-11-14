#include "IconFactory.h"

#include "IconFileEditIcons.h"
#include "IconLayerViewIcons.h"
#include "IconMergeIcons.h"
#include "IconShapeGraphicIcons.h"
#include "IconTransformAlignIcons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

void drawIconShape(QPainter& painter, IconId id)
{
    if (paintIconFileEdit(painter, id)) {
        return;
    }
    if (paintIconShapeGraphic(painter, id)) {
        return;
    }
    if (paintIconTransformAlign(painter, id)) {
        return;
    }
    if (paintIconLayerView(painter, id)) {
        return;
    }
    if (paintIconMerge(painter, id)) {
        return;
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
