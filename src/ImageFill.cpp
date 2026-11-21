#include "ImageFill.h"

#include <QBuffer>
#include <QByteArray>
#include <QJsonValue>
#include <QPainter>
#include <QtMath>

namespace ImageFill {

QString ModeToString(Mode mode)
{
    switch (mode) {
    case Mode::FIT:
        return QStringLiteral("fit");
    case Mode::TILE:
        return QStringLiteral("tile");
    case Mode::STRETCH:
    default:
        return QStringLiteral("stretch");
    }
}

Mode ModeFromString(const QString& name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("fit")) {
        return Mode::FIT;
    }
    if (normalized == QStringLiteral("tile")) {
        return Mode::TILE;
    }
    return Mode::STRETCH;
}

QJsonObject ToJson(const QPixmap& image, Mode mode)
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
    json.insert(QStringLiteral("mode"), ModeToString(mode));
    return json;
}

QPixmap PixmapFromJson(const QJsonObject& json, Mode* mode)
{
    if (mode) {
        *mode = ModeFromString(json.value(QStringLiteral("mode")).toString());
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
    case Mode::FIT: {
        const QSizeF imageSize(image.width(), image.height());
        const qreal scale = qMin(rect.width() / qMax<qreal>(1.0, imageSize.width()),
                                 rect.height() / qMax<qreal>(1.0, imageSize.height()));
        const QSizeF drawn = imageSize * scale;
        const QRectF target(rect.center().x() - drawn.width() / 2.0, rect.center().y() - drawn.height() / 2.0,
                            drawn.width(), drawn.height());
        painter->drawPixmap(target, image, QRectF(image.rect()));
        break;
    }
    case Mode::TILE: {
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
    case Mode::STRETCH:
    default:
        painter->drawPixmap(rect, image, QRectF(image.rect()));
        break;
    }
    painter->restore();
}

} // namespace ImageFill
