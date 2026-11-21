#pragma once

#include <QJsonObject>
#include <QPixmap>
#include <QSet>
#include <QString>

// 需求 11：画布级图片表。同一张图片在文件里只存一份（按内容哈希生成 id），
// 图元只保存 id 引用；保存时按「仍被引用」重建表，零引用的图片自然被丢掉。
// 模板 / 剪贴板 / 自定义图形这类自包含文件仍内联写 data，不走这张表。
namespace ImageStore {

// id 形如 "img-" + Sha1(PNG 字节) 前 16 位；相同像素必然得到相同 id。
QString IdFor(const QPixmap& image);

// 表项：{data: base64 PNG, mime: "image/png", w, h}。
QJsonObject EntryFromPixmap(const QPixmap& image);
QPixmap PixmapFromEntry(const QJsonObject& entry);

// 把图片收进表（已存在则直接复用 id），返回 id；空图返回空串。
QString Store(QJsonObject* table, const QPixmap& image);

// 只保留 usedIds 里的表项；保存画布前用它落盘。
QJsonObject Compact(const QJsonObject& table, const QSet<QString>& usedIds);

} // namespace ImageStore
