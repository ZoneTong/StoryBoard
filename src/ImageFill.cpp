#include "ImageFill.h"

#include <QBuffer>
#include <QByteArray>
#include <QJsonValue>
#include <QPainter>
#include <QtMath>

namespace ImageFill {

QString modeToString(Mode mode)
{
    switch (mode) {
    case Mode::Fit:
        return QStringLiteral("fit");
    case Mode::Tile:
        return QStringLiteral("tile");
    case Mode::Stretch:
    default:
        return QStringLiteral("stretch");
    }
}

Mode modeFromString(const QString& name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("fit")) {
        return Mode::Fit;
    }
    if (normalized == QStringLiteral("tile")) {
        return Mode::Tile;
    }
    return Mode::Stretch;
}

QJsonObject toJson(const QPixmap& image, Mode mode)
{
    if (image.isNull()) {
        return QJsonObject();
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    QJsonObject json;
    json.insert(QStringLiteral("data"), QString::fromLatin1(bytes.toBase64()));
    json.insert(QStringLiteral("mode"), modeToString(mode));
    return json;
}

QPixmap pixmapFromJson(const QJsonObject& json, Mode* mode)
{
    if (mode) {
        *mode = modeFromString(json.value(QStringLiteral("mode")).toString());
    }
    const QByteArray bytes = QByteArray::fromBase64(json.value(QStringLiteral("data")).toString().toLatin1());
    QPixmap pixmap;
    if (!bytes.isEmpty()) {
        pixmap.loadFromData(bytes, "PNG");
    }
    return pixmap;
}

void paint(QPainter* painter, const QPixmap& image, Mode mode, const QRectF& rect, const QPainterPath& clipPath)
{
    if (!painter || image.isNull() || rect.isEmpty() || clipPath.isEmpty()) {
        return;
    }
    painter->save();
    painter->setClipPath(clipPath, Qt::IntersectClip);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::NoBrush);

    switch (mode) {
    case Mode::Fit: {
        const QSizeF imageSize(image.width(), image.height());
        const qreal scale = qMin(rect.width() / qMax<qreal>(1.0, imageSize.width()),
                                 rect.height() / qMax<qreal>(1.0, imageSize.height()));
        const QSizeF drawn = imageSize * scale;
        const QRectF target(rect.center().x() - drawn.width() / 2.0, rect.center().y() - drawn.height() / 2.0,
                            drawn.width(), drawn.height());
        painter->drawPixmap(target, image, QRectF(image.rect()));
        break;
    }
    case Mode::Tile: {
        // 按原图像素 1:1 平铺：一个图片像素 = 一个画布单位，视图缩放时整体等比放大。
        const qreal tileWidth = qMax<qreal>(1.0, image.width());
        const qreal tileHeight = qMax<qreal>(1.0, image.height());
        const QRectF bounds = clipPath.boundingRect();
        const qreal startX = rect.left() + qFloor((bounds.left() - rect.left()) / tileWidth) * tileWidth;
        const qreal startY = rect.top() + qFloor((bounds.top() - rect.top()) / tileHeight) * tileHeight;
        for (qreal y = startY; y < bounds.bottom(); y += tileHeight) {
            for (qreal x = startX; x < bounds.right(); x += tileWidth) {
                painter->drawPixmap(QRectF(x, y, tileWidth, tileHeight), image, QRectF(image.rect()));
            }
        }
        break;
    }
    case Mode::Stretch:
    default:
        painter->drawPixmap(rect, image, QRectF(image.rect()));
        break;
    }
    painter->restore();
}

} // namespace ImageFill
