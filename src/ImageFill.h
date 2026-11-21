#pragma once

#include <QJsonObject>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

class QPainter;

// 需求 15：有填充区域的图元可以用图片当背景，图片只画在可填充区域内。
// 独立于 FillStyle 枚举：先画填充色 / 图案，图片铺在上面，图片透明区露出底层。
namespace ImageFill {

enum class Mode { STRETCH = 0, FIT, TILE };

QString ModeToString(Mode mode);
Mode ModeFromString(const QString& name);

// 序列化：{"data": <base64 PNG>, "mode": "stretch|fit|tile"}；无图时返回空对象，不写键。
QJsonObject ToJson(const QPixmap& image, Mode mode);
QPixmap PixmapFromJson(const QJsonObject& json, Mode* mode);

// rect 是图元内容矩形（图元局部坐标），clipPath 是可填充区域路径。
// Stretch 非等比铺满；Fit 等比留边居中；Tile 按图片原始像素 1:1 平铺。
void Paint(QPainter* painter, const QPixmap& image, Mode mode, const QRectF& rect, const QPainterPath& clipPath);

} // namespace ImageFill
