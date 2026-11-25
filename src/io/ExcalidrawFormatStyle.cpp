#include "io/ExcalidrawFormatInternal.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRectF>
#include <QString>
#include <QtGlobal>

namespace ExcalidrawFormatDetail {

qreal NumberOr(const QJsonObject& object, const QString& key, qreal fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toDouble() : fallback;
}

QColor ColorOr(const QJsonObject& object, const QString& key, const QColor& fallback)
{
    const QString text = object.value(key).toString();
    if (text.isEmpty() || text == QStringLiteral("transparent")) {
        return QColor(Qt::transparent);
    }
    const QColor color(text);
    return color.isValid() ? color : fallback;
}

// 线型双向往返：Excalidraw 只有 solid / dashed / dotted 三档，画板的点划线往返后退化成 dashed。
int PenStyleFromExcalidraw(const QJsonValue& value)
{
    const QString style = value.toString();
    if (style == QStringLiteral("dashed")) {
        return static_cast<int>(Qt::DashLine);
    }
    if (style == QStringLiteral("dotted")) {
        return static_cast<int>(Qt::DotLine);
    }
    return static_cast<int>(Qt::SolidLine);
}

QString ExcalidrawStrokeStyle(int penStyle)
{
    if (penStyle == static_cast<int>(Qt::DotLine)) {
        return QStringLiteral("dotted");
    }
    if (penStyle == static_cast<int>(Qt::DashLine) || penStyle == static_cast<int>(Qt::DashDotLine) ||
        penStyle == static_cast<int>(Qt::DashDotDotLine)) {
        return QStringLiteral("dashed");
    }
    return QStringLiteral("solid");
}

// 填充样式双向映射：Excalidraw 有 solid / hachure / cross-hatch / zigzag / dots / dashed，
// 画板另有反向斜纹、网格、蜂窝、砖墙四档，导出时落到最接近的原生样式并在 customData
// 里留下原值，再导入即可还原（customData 缺失时按最接近的画板样式落位）。
QString FillStyleFromExcalidraw(const QString& style)
{
    if (style == QStringLiteral("hachure") || style == QStringLiteral("dashed")) {
        return QStringLiteral("hachure");
    }
    if (style == QStringLiteral("cross-hatch") || style == QStringLiteral("zigzag") ||
        style == QStringLiteral("zigzag-line")) {
        return QStringLiteral("cross_hatch");
    }
    if (style == QStringLiteral("dots")) {
        return QStringLiteral("dots");
    }
    return QStringLiteral("solid");
}

QString ExcalidrawFillStyle(const QString& style)
{
    if (style == QStringLiteral("hachure") || style == QStringLiteral("reverse_hatch")) {
        return QStringLiteral("hachure");
    }
    if (style == QStringLiteral("dots")) {
        return QStringLiteral("dots");
    }
    if (style == QStringLiteral("brick")) {
        return QStringLiteral("zigzag");
    }
    if (style == QStringLiteral("cross_hatch") || style == QStringLiteral("grid_hatch") ||
        style == QStringLiteral("honeycomb")) {
        return QStringLiteral("cross-hatch");
    }
    return QStringLiteral("solid");
}

// 文字字体：Excalidraw 用 1/2/3 三个字体编号，画板按字体族名存，导出时按族名反查。
QString TextFontFamilyFromExcalidraw(const QJsonValue& value)
{
    const int id = value.toInt(1);
    if (id == 2) {
        return QStringLiteral("Virgil");
    }
    if (id == 3) {
        return QStringLiteral("Cascadia Code");
    }
    // 1 号是 Excalidraw 的默认无衬线字体，画板这边落到默认中文字体，避免中文回退成方框。
    return QStringLiteral("Microsoft YaHei UI");
}

int ExcalidrawFontFamily(const QString& family)
{
    const QString name = family.toLower();
    if (name.contains(QStringLiteral("virgil")) || name.contains(QStringLiteral("comic"))) {
        return 2;
    }
    if (name.contains(QStringLiteral("cascadia")) || name.contains(QStringLiteral("mono")) ||
        name.contains(QStringLiteral("consolas"))) {
        return 3;
    }
    return 1;
}

// 需求 9：Excalidraw 的 head 样式映射到画板箭头；空心三角画板有，直接保留。
// Excalidraw 的 head 取值比画板多：circle 画实心圆点、diamond 最近似实心三角，
// triangle_outline 落空心三角；cardinal / plus 画板没有，按无箭头处理。
QString HeadStyle(const QJsonValue& value)
{
    const QString type = value.isString() ? value.toString() : QString();
    if (type == QStringLiteral("bar")) {
        return QStringLiteral("bar");
    }
    if (type == QStringLiteral("dot") || type == QStringLiteral("circle") ||
        type == QStringLiteral("circle_outline")) {
        return QStringLiteral("dot");
    }
    if (type == QStringLiteral("triangle_outline")) {
        return QStringLiteral("hollow-triangle");
    }
    if (type == QStringLiteral("triangle") || type == QStringLiteral("diamond") ||
        type == QStringLiteral("diamond_outline")) {
        return QStringLiteral("triangle");
    }
    if (type == QStringLiteral("arrow")) {
        return QStringLiteral("open");
    }
    return QStringLiteral("none");
}

// 线条族的六个 kind：直线 / 弧线 / 角度线 / 正弦 / 手绘曲线 / 花括号。
bool IsLineKind(const QString& kind)
{
    return kind == QStringLiteral("straightline") || kind == QStringLiteral("arc") || kind == QStringLiteral("angle") ||
           kind == QStringLiteral("sine") || kind == QStringLiteral("freehand") || kind == QStringLiteral("brace");
}

QJsonObject ItemBase(const QString& kind, const QRectF& bounds, int z, int layer)
{
    QJsonObject item;
    item.insert(QStringLiteral("kind"), kind);
    item.insert(QStringLiteral("x"), bounds.x());
    item.insert(QStringLiteral("y"), bounds.y());
    item.insert(QStringLiteral("w"), bounds.width());
    item.insert(QStringLiteral("h"), bounds.height());
    item.insert(QStringLiteral("layer"), layer);
    item.insert(QStringLiteral("height"), 1);
    item.insert(QStringLiteral("z"), z);
    return item;
}

QString HeadValue(const QJsonObject& item, const QString& key)
{
    const QString style = item.value(key).toString();
    if (style == QStringLiteral("bar")) {
        return QStringLiteral("bar");
    }
    if (style == QStringLiteral("dot")) {
        return QStringLiteral("dot");
    }
    if (style == QStringLiteral("triangle") || style == QStringLiteral("hollow-triangle")) {
        return QStringLiteral("triangle");
    }
    if (style == QStringLiteral("open")) {
        return QStringLiteral("arrow");
    }
    return QString();
}

// 导出时把画板独有的样式塞进 customData：填充样式、填充/描线 alpha、空心三角箭头标记。
QJsonObject StyleCustomData(const QJsonObject& item)
{
    QJsonObject custom;
    custom.insert(QStringLiteral("fillStyle"), item.value(QStringLiteral("fillStyle")).toString());
    custom.insert(QStringLiteral("fillAlpha"),
                  ColorOr(item, QStringLiteral("fillColor"), QColor(Qt::transparent)).alpha());
    custom.insert(QStringLiteral("strokeAlpha"),
                  ColorOr(item, QStringLiteral("strokeColor"), QColor(30, 41, 59)).alpha());
    return custom;
}

QJsonObject HeadCustomData(const QJsonObject& item)
{
    QJsonObject custom;
    if (item.value(QStringLiteral("startArrow")).toString() == QStringLiteral("hollow-triangle")) {
        custom.insert(QStringLiteral("startArrowStyle"), QStringLiteral("hollow-triangle"));
    }
    if (item.value(QStringLiteral("endArrow")).toString() == QStringLiteral("hollow-triangle")) {
        custom.insert(QStringLiteral("endArrowStyle"), QStringLiteral("hollow-triangle"));
    }
    return custom;
}

} // namespace ExcalidrawFormatDetail
