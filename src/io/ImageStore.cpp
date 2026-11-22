#include "io/ImageStore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonValue>

namespace {

// 唯一的编码入口：所有 id 与 data 都基于同一次 PNG 编码结果，保证内容哈希一致。
QByteArray EncodePng(const QPixmap& image)
{
    if (image.isNull()) {
        return QByteArray();
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.toImage().save(&buffer, "PNG");
    return bytes;
}

} // namespace

QString ImageStore::IdFor(const QPixmap& image)
{
    const QByteArray bytes = EncodePng(image);
    if (bytes.isEmpty()) {
        return QString();
    }
    const QString digest =
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().left(16));
    return QStringLiteral("img-") + digest;
}

QJsonObject ImageStore::EntryFromPixmap(const QPixmap& image)
{
    QJsonObject entry;
    const QByteArray bytes = EncodePng(image);
    if (bytes.isEmpty()) {
        return entry;
    }
    entry.insert(QStringLiteral("data"), QString::fromLatin1(bytes.toBase64()));
    entry.insert(QStringLiteral("mime"), QStringLiteral("image/png"));
    entry.insert(QStringLiteral("w"), image.width());
    entry.insert(QStringLiteral("h"), image.height());
    return entry;
}

QPixmap ImageStore::PixmapFromEntry(const QJsonObject& entry)
{
    const QString data = entry.value(QStringLiteral("data")).toString();
    if (data.isEmpty()) {
        return QPixmap();
    }
    const QByteArray bytes = QByteArray::fromBase64(data.toLatin1());
    QImage image;
    if (!image.loadFromData(bytes)) {
        return QPixmap();
    }
    return QPixmap::fromImage(image);
}

QString ImageStore::Store(QJsonObject* table, const QPixmap& image)
{
    const QString id = IdFor(image);
    if (id.isEmpty() || !table) {
        return id;
    }
    if (!table->contains(id)) {
        table->insert(id, EntryFromPixmap(image));
    }
    return id;
}

QJsonObject ImageStore::Compact(const QJsonObject& table, const QSet<QString>& usedIds)
{
    QJsonObject result;
    for (const QString& id : usedIds) {
        if (table.contains(id)) {
            result.insert(id, table.value(id));
        }
    }
    return result;
}
