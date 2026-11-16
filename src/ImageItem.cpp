#include "ImageItem.h"

#include "I18n.h"

#include <QBuffer>
#include <QPainter>

ImageItem::ImageItem(QGraphicsItem* parent) : CanvasItem(parent) {}

QString ImageItem::displayName() const
{
    return m_name.isEmpty() ? tr("Image") : m_name;
}

QString ImageItem::typeDisplayName() const
{
    return tr("Image");
}

void ImageItem::setImage(const QPixmap& pixmap, const QString& name)
{
    m_pixmap = pixmap;
    m_name = name;
    update();
}

void ImageItem::setImageName(const QString& name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    update();
}

void ImageItem::paintContent(QPainter* painter)
{
    if (m_pixmap.isNull()) {
        painter->setPen(QPen(QColor(148, 163, 184), 1.0, Qt::DashLine));
        painter->setBrush(QColor(248, 250, 252));
        painter->drawRoundedRect(contentRect(), 4.0, 4.0);
        painter->drawText(contentRect(), Qt::AlignCenter, tr("Missing image"));
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
    } else if (!m_sourceBytes.isEmpty()) {
        // 位图解码失败但原始字节还在（损坏 / 暂不支持的格式）：原样写回，
        // 不把文件里仅存的图片数据替换成空值。
        json.insert(QStringLiteral("image"), QString::fromLatin1(m_sourceBytes.toBase64()));
    }
    return json;
}

void ImageItem::fromJson(const QJsonObject& json)
{
    m_name = json.value(QStringLiteral("name")).toString();
    m_sourceBytes = QByteArray::fromBase64(json.value(QStringLiteral("image")).toString().toLatin1());

    QPixmap pixmap;
    if (!m_sourceBytes.isEmpty()) {
        pixmap.loadFromData(m_sourceBytes, "PNG");
    }
    m_pixmap = pixmap;
    applyBaseJson(json);
}
