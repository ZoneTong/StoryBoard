#include "I18n.h"

#include "ImageItem.h"

#include <QBuffer>
#include <QPainter>

ImageItem::ImageItem(QGraphicsItem* parent)
    : BoardItem(parent)
{
}

QString ImageItem::displayName() const
{
    return m_name.isEmpty() ? uiText(QStringLiteral("图片")) : m_name;
}

void ImageItem::setImage(const QPixmap& pixmap, const QString& name)
{
    m_pixmap = pixmap;
    m_name = name;
    update();
}

void ImageItem::paintContent(QPainter* painter)
{
    if (m_pixmap.isNull()) {
        painter->setPen(QPen(QColor(148, 163, 184), 1.0, Qt::DashLine));
        painter->setBrush(QColor(248, 250, 252));
        painter->drawRoundedRect(contentRect(), 4.0, 4.0);
        painter->drawText(contentRect(), Qt::AlignCenter, uiText(QStringLiteral("图片缺失")));
        return;
    }

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawPixmap(contentRect(), m_pixmap, QRectF(m_pixmap.rect()));
}

QJsonObject ImageItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("name"), m_name);

    if (!m_pixmap.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        m_pixmap.save(&buffer, "PNG");
        json.insert(QStringLiteral("image"), QString::fromLatin1(bytes.toBase64()));
    }
    return json;
}

void ImageItem::fromJson(const QJsonObject& json)
{
    m_name = json.value(QStringLiteral("name")).toString();
    const QByteArray bytes = QByteArray::fromBase64(
        json.value(QStringLiteral("image")).toString().toLatin1());

    QPixmap pixmap;
    if (!bytes.isEmpty())
        pixmap.loadFromData(bytes, "PNG");
    m_pixmap = pixmap;
    applyBaseJson(json);
}
