#include "io/ExcalidrawFormat.h"

#include "items/properties/TextAttributes.h"

#include <QColor>
#include <QHash>
#include <QJsonDocument>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QtMath>
#include <utility>

namespace {

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
    return QStringLiteral("Helvetica");
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

// 单条折线最多保留的点数：再多对画板没有意义，只会把曲线图元撑爆。
constexpr int MAX_POINTS = 10000;

// segments 在画板里是“相对图元原点”的本地坐标，而 Excalidraw 的点是世界坐标；
// origin 传包围盒左上角（= 图元 x/y），否则整条线会再平移一次、落到画布外。
QJsonArray PolylineSegments(const QList<QPointF>& points, const QPointF& origin = QPointF())
{
    QJsonArray segments;
    const int last = qMin(points.size() - 1, MAX_POINTS - 1);
    for (int i = 1; i <= last; ++i) {
        const QPointF p0 = points.at(i - 1) - origin;
        const QPointF p1 = points.at(i) - origin;
        QJsonArray values;
        values.append(p0.x());
        values.append(p0.y());
        values.append(p0.x() + (p1.x() - p0.x()) / 3.0);
        values.append(p0.y() + (p1.y() - p0.y()) / 3.0);
        values.append(p0.x() + (p1.x() - p0.x()) * 2.0 / 3.0);
        values.append(p0.y() + (p1.y() - p0.y()) * 2.0 / 3.0);
        values.append(p1.x());
        values.append(p1.y());
        segments.append(values);
    }
    return segments;
}

QList<QPointF> AbsolutePoints(const QJsonObject& element, QStringList* warnings)
{
    QList<QPointF> points;
    const QJsonArray raw = element.value(QStringLiteral("points")).toArray();
    for (const QJsonValue& value : raw) {
        // freedraw 的点可能成千上万，超出上限后不再解析。
        if (points.size() >= MAX_POINTS) {
            if (warnings) {
                warnings->append(QStringLiteral("点过多（%1），只取前 %2 个").arg(raw.size()).arg(MAX_POINTS));
            }
            break;
        }
        const QJsonArray pair = value.toArray();
        if (pair.size() < 2) {
            continue;
        }
        points.append(QPointF(pair.at(0).toDouble() + NumberOr(element, QStringLiteral("x"), 0.0),
                              pair.at(1).toDouble() + NumberOr(element, QStringLiteral("y"), 0.0)));
    }
    return points;
}

// 水平/垂直直线（或全等点）的包围盒高或宽为 0，不能用 QRectF::isNull() 当“首个点”哨兵，
// 否则整条线会退化到末点、宽高归零，粘贴后在图层里能看到名字却画不出来。
QRectF BoundsOf(const QList<QPointF>& points)
{
    if (points.isEmpty()) {
        return QRectF();
    }
    qreal minX = points.constFirst().x();
    qreal maxX = minX;
    qreal minY = points.constFirst().y();
    qreal maxY = minY;
    for (const QPointF& point : points) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

// 导出时写进 customData 的原样式字段；导入先读这里，读不到再按 Excalidraw 语义近似。
QString CustomText(const QJsonObject& element, const QString& key)
{
    return element.value(QStringLiteral("customData")).toObject().value(key).toString();
}

qreal CustomNumber(const QJsonObject& element, const QString& key, qreal fallback)
{
    return NumberOr(element.value(QStringLiteral("customData")).toObject(), key, fallback);
}

// 箭头样式：空心三角在 Excalidraw 里只能写成 triangle，靠 customData 还原。
QString ImportHeadStyle(const QJsonObject& element, const QString& key)
{
    const QString restored = CustomText(element, key + QStringLiteral("Style"));
    if (restored == QStringLiteral("hollow-triangle")) {
        return restored;
    }
    return HeadStyle(element.value(key + QStringLiteral("head")));
}

QJsonObject ElementToItem(const QJsonObject& element, int z, int layer, QStringList* warnings, const QJsonObject& files,
                          bool keepImages)
{
    const QString type = element.value(QStringLiteral("type")).toString();
    const QRectF bounds(NumberOr(element, QStringLiteral("x"), 0.0), NumberOr(element, QStringLiteral("y"), 0.0),
                        NumberOr(element, QStringLiteral("width"), 0.0),
                        NumberOr(element, QStringLiteral("height"), 0.0));

    if (type == QStringLiteral("image")) {
        // 勾选位图保留时，按 Excalidraw 的 dataURL 落成图片图元；否则跳过并提示。
        if (!keepImages) {
            if (warnings) {
                warnings->append(QStringLiteral("跳过 image 元素（未勾选位图保留）"));
            }
            return {};
        }
        const QString dataUrl = files.value(element.value(QStringLiteral("fileId")).toString())
                                    .toObject()
                                    .value(QStringLiteral("dataURL"))
                                    .toString();
        const int comma = dataUrl.indexOf(QLatin1Char(','));
        if (comma < 0) {
            if (warnings) {
                warnings->append(QStringLiteral("image 元素缺少 dataURL，已跳过"));
            }
            return {};
        }
        // 画板没有独立的位图图元，落到“矩形 + 图片填充”：Excalidraw 的 image 本身就是
        // 一个矩形框，这样导入后能直接看见、移动缩放，再导出也还是 image 元素。
        QJsonObject item = ItemBase(QStringLiteral("shape"), bounds, z, layer);
        item.insert(QStringLiteral("shapeType"), QStringLiteral("rectangle"));
        item.insert(QStringLiteral("strokeColor"), QStringLiteral("#00000000"));
        item.insert(QStringLiteral("fillColor"), QStringLiteral("#00000000"));
        item.insert(QStringLiteral("fillStyle"), QStringLiteral("none"));
        item.insert(QStringLiteral("strokeWidth"), 1.0);
        item.insert(QStringLiteral("name"), element.value(QStringLiteral("id")).toString());
        const QString data = dataUrl.mid(comma + 1);
        QJsonObject fillImage;
        fillImage.insert(QStringLiteral("data"), data);
        fillImage.insert(QStringLiteral("mode"), QStringLiteral("stretch"));
        item.insert(QStringLiteral("fillImage"), fillImage);
        item.insert(QStringLiteral("opacity"), NumberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
        item.insert(QStringLiteral("rotation"), qRadiansToDegrees(NumberOr(element, QStringLiteral("angle"), 0.0)));
        return item;
    }

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("diamond")) {
        // 菱形（Excalidraw 原生类型）直接落成独立菱形类型：外接框宽即水平对角线。
        QJsonObject item = ItemBase(QStringLiteral("shape"), bounds, z, layer);
        // 导出时记下的原图形类型优先，三角形 / 星形之类才不会退回矩形。
        const QString restoredType = CustomText(element, QStringLiteral("storyboardShapeType"));
        QString shapeType = type == QStringLiteral("ellipse")   ? QStringLiteral("ellipse")
                            : type == QStringLiteral("diamond") ? QStringLiteral("diamond")
                                                                : QStringLiteral("rectangle");
        if (!restoredType.isEmpty()) {
            shapeType = restoredType;
        }
        item.insert(QStringLiteral("shapeType"), shapeType);
        QColor stroke = ColorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59));
        QColor fill = ColorOr(element, QStringLiteral("backgroundColor"), QColor(Qt::transparent));
        QString fillStyle = QStringLiteral("none");
        if (fill.alpha() > 0) {
            // 原填充样式（斜纹 / 网格 / 蜂窝等）在 customData 里，取不到就按最接近的样式落位。
            const QString restoredStyle = CustomText(element, QStringLiteral("fillStyle"));
            if (!restoredStyle.isEmpty()) {
                fillStyle = restoredStyle;
            } else {
                const QString sourceStyle = element.value(QStringLiteral("fillStyle")).toString();
                fillStyle = FillStyleFromExcalidraw(sourceStyle);
                if (!sourceStyle.isEmpty() && sourceStyle != QStringLiteral("solid") &&
                    sourceStyle != QStringLiteral("hachure") && sourceStyle != QStringLiteral("cross-hatch") &&
                    sourceStyle != QStringLiteral("zigzag") && sourceStyle != QStringLiteral("zigzag-line") &&
                    sourceStyle != QStringLiteral("dots") && sourceStyle != QStringLiteral("dashed") && warnings) {
                    warnings->append(QStringLiteral("fillStyle %1 不支持，按实心填充").arg(sourceStyle));
                }
            }
        }
        // 透明度：画板把透明度放在颜色 alpha 上，Excalidraw 的颜色不带 alpha，靠 customData 还原。
        const qreal fillAlpha = CustomNumber(element, QStringLiteral("fillAlpha"), fill.alpha());
        if (fill.alpha() > 0) {
            fill.setAlpha(static_cast<int>(qBound<qreal>(0.0, fillAlpha, 255.0)));
        }
        const qreal strokeAlpha = CustomNumber(element, QStringLiteral("strokeAlpha"), stroke.alpha());
        stroke.setAlpha(static_cast<int>(qBound<qreal>(1.0, strokeAlpha, 255.0)));
        item.insert(QStringLiteral("strokeColor"), stroke.name(QColor::HexArgb));
        item.insert(QStringLiteral("fillColor"), fill.name(QColor::HexArgb));
        item.insert(QStringLiteral("fillStyle"), fillStyle);
        item.insert(QStringLiteral("strokeWidth"),
                    qMax<qreal>(0.5, NumberOr(element, QStringLiteral("strokeWidth"), 2.0)));
        item.insert(QStringLiteral("strokeStyle"),
                    PenStyleFromExcalidraw(element.value(QStringLiteral("strokeStyle"))));
        item.insert(QStringLiteral("opacity"), NumberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
        item.insert(QStringLiteral("cornerRadius"),
                    qMax<qreal>(0.0, NumberOr(element.value(QStringLiteral("roundness")).toObject(),
                                              QStringLiteral("value"), 0.0)));
        item.insert(QStringLiteral("rotation"), qRadiansToDegrees(NumberOr(element, QStringLiteral("angle"), 0.0)));
        return item;
    }

    if (type == QStringLiteral("text")) {
        const QString content = element.value(QStringLiteral("text")).toString();
        // 需求 9：Excalidraw 字体用 px，画板用 pt，约 1.333 倍换算。
        const qreal pointSize = qMax<qreal>(6.0, NumberOr(element, QStringLiteral("fontSize"), 20.0) / 1.333);
        // Excalidraw 的文字框不含画板的 12px 内边距，字号也来自另一套字体；
        // 直接照抄外框会让首行基线落在框外、下半截被裁剪。这里按画板字体重新量自然尺寸。
        const QSizeF natural = TextAttributes::Measure(content, TextAttributes::DefaultFont(pointSize));
        // 对齐方式画板不支持，改成按 Excalidraw 的对齐位置平移，保证导入后的视觉位置一致。
        qreal shiftX = 0.0;
        qreal shiftY = 0.0;
        const QString textAlign = element.value(QStringLiteral("textAlign")).toString();
        if (textAlign == QStringLiteral("center")) {
            shiftX = (bounds.width() - natural.width()) / 2.0;
        } else if (textAlign == QStringLiteral("right")) {
            shiftX = bounds.width() - natural.width();
        }
        const QString verticalAlign = element.value(QStringLiteral("verticalAlign")).toString();
        if (verticalAlign == QStringLiteral("middle")) {
            shiftY = (bounds.height() - natural.height()) / 2.0;
        } else if (verticalAlign == QStringLiteral("bottom")) {
            shiftY = bounds.height() - natural.height();
        }
        QJsonObject item = ItemBase(QStringLiteral("text"), QRectF(bounds.topLeft() + QPointF(shiftX, shiftY), natural), z,
                                    layer);
        item.insert(QStringLiteral("text"), content);
        item.insert(QStringLiteral("color"),
                    ColorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexArgb));
        item.insert(QStringLiteral("fontSize"), pointSize);
        item.insert(QStringLiteral("fontFamily"), TextFontFamilyFromExcalidraw(element.value(QStringLiteral("fontFamily"))));
        item.insert(QStringLiteral("baseWidth"), natural.width());
        item.insert(QStringLiteral("baseHeight"), natural.height());
        item.insert(QStringLiteral("opacity"), NumberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
        item.insert(QStringLiteral("rotation"), qRadiansToDegrees(NumberOr(element, QStringLiteral("angle"), 0.0)));
        return item;
    }

    if (type == QStringLiteral("line") || type == QStringLiteral("arrow") || type == QStringLiteral("freedraw")) {
        const QList<QPointF> points = AbsolutePoints(element, warnings);
        if (points.size() < 2) {
            return {};
        }
        // 多点折线：保留拐点与两端箭头，落到直线图元（自由笔画落到曲线图元）。
        const QRectF lineBounds = BoundsOf(points);
        const QString lineKind =
            type == QStringLiteral("freedraw") ? QStringLiteral("freehand") : QStringLiteral("straightline");
        QJsonObject item = ItemBase(lineKind, lineBounds, z, layer);
        item.insert(QStringLiteral("color"),
                    ColorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexArgb));
        item.insert(QStringLiteral("width"), qMax<qreal>(0.5, NumberOr(element, QStringLiteral("strokeWidth"), 2.0)));
        item.insert(QStringLiteral("strokeStyle"),
                    PenStyleFromExcalidraw(element.value(QStringLiteral("strokeStyle"))));
        item.insert(QStringLiteral("opacity"), NumberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
        item.insert(QStringLiteral("baseWidth"), lineBounds.width());
        item.insert(QStringLiteral("baseHeight"), lineBounds.height());
        item.insert(QStringLiteral("dot"), false);
        item.insert(QStringLiteral("linear"), true);
        // 类型标识只写一次：手绘笔迹为 freehand，其余折线为 straightline。
        item.insert(QStringLiteral("startArrow"), ImportHeadStyle(element, QStringLiteral("startArrow")));
        item.insert(QStringLiteral("endArrow"), ImportHeadStyle(element, QStringLiteral("endArrow")));
        item.insert(QStringLiteral("segments"), PolylineSegments(points, lineBounds.topLeft()));
        return item;
    }

    if (warnings && type != QStringLiteral("frame")) {
        warnings->append(QStringLiteral("跳过不支持的 Excalidraw 元素类型 %1").arg(type));
    }
    return {};
}

QJsonObject StoryBoardShape(const QJsonObject& item)
{
    const QString shapeType = item.value(QStringLiteral("shapeType")).toString();
    const QString type =
        shapeType == QStringLiteral("ellipse") ? QStringLiteral("ellipse") : QStringLiteral("rectangle");
    QJsonObject element;
    element.insert(QStringLiteral("type"), type);
    element.insert(QStringLiteral("strokeColor"),
                   ColorOr(item, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexRgb));
    const QColor fill = ColorOr(item, QStringLiteral("fillColor"), QColor(Qt::transparent));
    element.insert(QStringLiteral("backgroundColor"),
                   fill.alpha() == 0 ? QStringLiteral("transparent") : fill.name(QColor::HexRgb));
    element.insert(QStringLiteral("fillStyle"),
                   ExcalidrawFillStyle(item.value(QStringLiteral("fillStyle")).toString()));
    element.insert(QStringLiteral("strokeWidth"), NumberOr(item, QStringLiteral("strokeWidth"), 2.0));
    // 圆角：Excalidraw 的合法值是 null 或 {type:3,value}，空对象属于无效值。
    const qreal corner = NumberOr(item, QStringLiteral("cornerRadius"), 0.0);
    if (corner > 0.0) {
        QJsonObject roundness;
        roundness.insert(QStringLiteral("type"), 3);
        roundness.insert(QStringLiteral("value"), corner);
        element.insert(QStringLiteral("roundness"), roundness);
    } else {
        element.insert(QStringLiteral("roundness"), QJsonValue::Null);
    }
    return element;
}

QJsonObject BaseElement(const QJsonObject& item, const QString& id)
{
    QJsonObject element;
    element.insert(QStringLiteral("id"), id);
    element.insert(QStringLiteral("x"), NumberOr(item, QStringLiteral("x"), 0.0));
    element.insert(QStringLiteral("y"), NumberOr(item, QStringLiteral("y"), 0.0));
    element.insert(QStringLiteral("width"), NumberOr(item, QStringLiteral("w"), 0.0));
    element.insert(QStringLiteral("height"), NumberOr(item, QStringLiteral("h"), 0.0));
    element.insert(QStringLiteral("angle"), qDegreesToRadians(NumberOr(item, QStringLiteral("rotation"), 0.0)));
    element.insert(QStringLiteral("opacity"), NumberOr(item, QStringLiteral("opacity"), 1.0) * 100.0);
    element.insert(QStringLiteral("strokeStyle"),
                   ExcalidrawStrokeStyle(static_cast<int>(NumberOr(item, QStringLiteral("strokeStyle"), 1.0))));
    element.insert(QStringLiteral("roughness"), 0);
    element.insert(QStringLiteral("seed"), static_cast<int>(id.right(4).toInt(nullptr, 16)));
    element.insert(QStringLiteral("version"), 1);
    element.insert(QStringLiteral("versionNonce"), 1);
    element.insert(QStringLiteral("isDeleted"), false);
    element.insert(QStringLiteral("groupIds"), QJsonArray{});
    element.insert(QStringLiteral("boundElements"), QJsonArray{});
    element.insert(QStringLiteral("updated"), 1);
    element.insert(QStringLiteral("link"), QJsonValue::Null);
    element.insert(QStringLiteral("locked"), false);
    element.insert(QStringLiteral("frameId"), QJsonValue::Null);
    return element;
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

QJsonArray RelativePoints(const QJsonArray& segments, qreal originX, qreal originY)
{
    QList<QPointF> points;
    for (const QJsonValue& value : segments) {
        const QJsonArray numbers = value.toArray();
        if (numbers.size() < 8) {
            continue;
        }
        points.append(QPointF(numbers.at(0).toDouble(), numbers.at(1).toDouble()));
        points.append(QPointF(numbers.at(6).toDouble(), numbers.at(7).toDouble()));
    }
    QJsonArray result;
    for (const QPointF& point : std::as_const(points)) {
        QJsonArray pair;
        pair.append(point.x() - originX);
        pair.append(point.y() - originY);
        result.append(pair);
    }
    return result;
}

} // namespace

namespace Excalidraw {

bool IsScene(const QJsonObject& root)
{
    return root.value(QStringLiteral("type")).toString() == QLatin1String(SCENE_TYPE) &&
           root.value(QStringLiteral("elements")).isArray();
}

bool IsClipboard(const QJsonObject& root)
{
    return root.value(QStringLiteral("type")).toString() == QLatin1String(CLIPBOARD_TYPE) &&
           root.value(QStringLiteral("elements")).isArray();
}

QJsonArray ElementsToItems(const QJsonArray& elements, QStringList* warnings, QJsonArray* layers,
                           const QJsonObject& files, bool keepImages, QJsonObject* groupParents)
{
    QJsonArray items;
    int z = 1000001;
    // 需求 2：导入闸门。元素数超上限时只取前 20000 个，避免恶意或超大场景卡死界面。
    constexpr int MAX_ELEMENTS = 20000;
    if (elements.size() > MAX_ELEMENTS) {
        if (warnings) {
            warnings->append(QStringLiteral("Excalidraw 元素过多，只导入前 %1 个（共 %2 个）")
                                 .arg(MAX_ELEMENTS)
                                 .arg(elements.size()));
        }
    }

    // frame -> 图层号。1 号层留给不属于任何 frame 的元素，frame 从 2 号开始依次编号。
    QHash<QString, int> frameLayer;
    QJsonArray frameLayers;
    int nextLayer = 2;
    int frameIndex = 0;
    for (const QJsonValue& value : elements) {
        const QJsonObject element = value.toObject();
        if (element.value(QStringLiteral("type")).toString() != QLatin1String("frame") ||
            element.value(QStringLiteral("isDeleted")).toBool(false)) {
            continue;
        }
        const QString id = element.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || frameLayer.contains(id)) {
            continue;
        }
        ++frameIndex;
        QJsonObject layer;
        layer.insert(QStringLiteral("id"), nextLayer);
        const QString name = element.value(QStringLiteral("name")).toString();
        // 无名称的 frame 用 frame-<n>，n 按 frame 在场景里出现的次序。
        layer.insert(QStringLiteral("name"), name.isEmpty() ? QStringLiteral("frame-%1").arg(frameIndex) : name);
        layer.insert(QStringLiteral("visible"), true);
        layer.insert(QStringLiteral("locked"), false);
        frameLayers.append(layer);
        frameLayer.insert(id, nextLayer);
        ++nextLayer;
    }

    // groupIds -> 画板组号：Excalidraw 的 groupIds 从内到外排列，内层给 item 的 group，
    // 其余写进 groupParents 组成嵌套链，还原画板的组层级。
    QHash<QString, int> groupIdNumbers;
    int nextGroup = 1;
    QHash<int, bool> usedGroupNumbers;
    QJsonObject parentRecords;

    for (const QJsonValue& value : elements) {
        if (items.size() >= MAX_ELEMENTS) {
            break;
        }
        const QJsonObject element = value.toObject();
        if (element.value(QStringLiteral("isDeleted")).toBool(false)) {
            continue;
        }
        if (element.value(QStringLiteral("type")).toString() == QLatin1String("frame")) {
            // frame 本身只是分组容器，已经变成图层，不再生成图元。
            continue;
        }
        const QJsonArray groupChain = element.value(QStringLiteral("groupIds")).toArray();
        QList<int> chain;
        for (const QJsonValue& groupValue : groupChain) {
            const QString groupId = groupValue.toString();
            if (groupId.isEmpty()) {
                continue;
            }
            if (!groupIdNumbers.contains(groupId)) {
                // 画板自己导出的组 id 就是数字串，直接沿用让组号往返不变；
                // 外部 Excalidraw 的随机 id 才自增分配，并避开已占用的数字。
                bool numeric = false;
                const int wanted = groupId.toInt(&numeric);
                int number = 0;
                if (numeric && wanted > 0 && !usedGroupNumbers.contains(wanted)) {
                    number = wanted;
                } else {
                    while (usedGroupNumbers.contains(nextGroup)) {
                        ++nextGroup;
                    }
                    number = nextGroup;
                    ++nextGroup;
                }
                usedGroupNumbers.insert(number, true);
                groupIdNumbers.insert(groupId, number);
            }
            chain.append(groupIdNumbers.value(groupId));
        }
        for (int index = 0; index + 1 < chain.size(); ++index) {
            if (chain.at(index) > 0 && chain.at(index + 1) > 0) {
                parentRecords.insert(QString::number(chain.at(index)), chain.at(index + 1));
            }
        }
        const int layer = frameLayer.value(element.value(QStringLiteral("frameId")).toString(), 1);
        QJsonObject item = ElementToItem(element, z, layer, warnings, files, keepImages);
        if (item.isEmpty()) {
            continue;
        }
        if (!chain.isEmpty()) {
            item.insert(QStringLiteral("group"), chain.constFirst());
        }
        items.append(item);
        ++z;
    }

    if (groupParents) {
        *groupParents = parentRecords;
    }

    if (layers) {
        QJsonObject base;
        base.insert(QStringLiteral("id"), 1);
        base.insert(QStringLiteral("name"), QStringLiteral("图层 1"));
        base.insert(QStringLiteral("visible"), true);
        base.insert(QStringLiteral("locked"), false);
        // 按层号排序：默认层在最前，frame 层从 2 号起顺序追加。
        QJsonArray sorted{base};
        for (const QJsonValue& value : frameLayers) {
            sorted.append(value);
        }
        *layers = sorted;
    }
    return items;
}

QJsonObject SceneToStoryBoard(const QJsonObject& scene, QStringList* warnings, bool keepImages)
{
    QJsonArray layers;
    QJsonObject groupParents;
    const QJsonArray items = ElementsToItems(scene.value(QStringLiteral("elements")).toArray(), warnings, &layers,
                                             scene.value(QStringLiteral("files")).toObject(), keepImages, &groupParents);
    if (items.isEmpty()) {
        return {};
    }

    // 画布尺寸取所有元素的包围盒并留边，Excalidraw 场景本身没有画布尺寸。
    qreal right = 0.0;
    qreal bottom = 0.0;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        right = qMax(right, item.value(QStringLiteral("x")).toDouble() + item.value(QStringLiteral("w")).toDouble());
        bottom = qMax(bottom, item.value(QStringLiteral("y")).toDouble() + item.value(QStringLiteral("h")).toDouble());
    }

    QJsonObject document;
    document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    document.insert(QStringLiteral("version"), 2);
    document.insert(QStringLiteral("generator"), QStringLiteral("StoryBoard Excalidraw import"));
    QJsonObject canvas;
    canvas.insert(QStringLiteral("x"), 0);
    canvas.insert(QStringLiteral("y"), 0);
    canvas.insert(QStringLiteral("width"), qMax<qreal>(512.0, right + 64.0));
    canvas.insert(QStringLiteral("height"), qMax<qreal>(512.0, bottom + 64.0));
    document.insert(QStringLiteral("canvas"), canvas);

    const QString background =
        scene.value(QStringLiteral("appState")).toObject().value(QStringLiteral("viewBackgroundColor")).toString();
    QJsonObject backgroundObject;
    if (background.isEmpty() || background == QStringLiteral("transparent")) {
        backgroundObject.insert(QStringLiteral("type"), QStringLiteral("none"));
    } else {
        backgroundObject.insert(QStringLiteral("type"), QStringLiteral("color"));
        backgroundObject.insert(QStringLiteral("color"), background);
    }
    document.insert(QStringLiteral("background"), backgroundObject);

    document.insert(QStringLiteral("layers"), layers);
    if (!groupParents.isEmpty()) {
        document.insert(QStringLiteral("groupParents"), groupParents);
    }
    document.insert(QStringLiteral("activeLayer"), 1);
    document.insert(QStringLiteral("items"), items);
    return document;
}

QJsonObject StoryBoardToScene(const QJsonObject& document, QStringList* warnings)
{
    const QJsonArray items = document.value(QStringLiteral("items")).toArray();
    QJsonArray elements;
    int index = 0;
    // 组的嵌套链：item.group 是内层，沿 groupParents 一路往上写进 groupIds。
    QHash<int, int> groupParents;
    const QJsonObject parentsJson = document.value(QStringLiteral("groupParents")).toObject();
    for (auto it = parentsJson.constBegin(); it != parentsJson.constEnd(); ++it) {
        groupParents.insert(it.key().toInt(), it.value().toInt());
    }
    // 图片填充：矩形可以直接还原成 Excalidraw 的 image 元素（图片本身就是矩形框），
    // 其它形状保留原有的“降级为纯色”提示。图片数据可能内联在 item，也可能在根 images 表。
    const QJsonObject imageEntries = document.value(QStringLiteral("images")).toObject();
    auto ImageDataFor = [&imageEntries](const QJsonObject& item) -> QString {
        const QJsonObject inlineFill = item.value(QStringLiteral("fillImage")).toObject();
        if (!inlineFill.isEmpty()) {
            return inlineFill.value(QStringLiteral("data")).toString();
        }
        const QString id = item.value(QStringLiteral("fillImageId")).toString();
        if (!id.isEmpty()) {
            return imageEntries.value(id).toObject().value(QStringLiteral("data")).toString();
        }
        return QString();
    };
    QJsonObject filesTable;
    int imageFileIndex = 0;
    // 图层 -> Excalidraw frame：每个非默认层生成一个 frame，层内图元写 frameId，
    // frame 外框取该层图元包围盒外扩 10px；再导入时 frame 又还原成图层。
    QHash<int, QString> frameIds;
    QJsonArray frameElements;
    {
        const QJsonArray layersJson = document.value(QStringLiteral("layers")).toArray();
        for (const QJsonValue& layerValue : layersJson) {
            const QJsonObject layer = layerValue.toObject();
            const int layerId = layer.value(QStringLiteral("id")).toInt(0);
            if (layerId <= 1) {
                continue;
            }
            QRectF bounds;
            bool first = true;
            for (const QJsonValue& itemValue : items) {
                const QJsonObject item = itemValue.toObject();
                if (item.value(QStringLiteral("layer")).toInt(0) != layerId) {
                    continue;
                }
                const QRectF itemRect(NumberOr(item, QStringLiteral("x"), 0.0), NumberOr(item, QStringLiteral("y"), 0.0),
                                      NumberOr(item, QStringLiteral("w"), 0.0), NumberOr(item, QStringLiteral("h"), 0.0));
                bounds = first ? itemRect : bounds.united(itemRect);
                first = false;
            }
            if (first) {
                continue;
            }
            bounds = bounds.adjusted(-10.0, -10.0, 10.0, 10.0);
            const QString frameId = QStringLiteral("sbframe%1").arg(layerId, 4, 10, QLatin1Char('0'));
            QJsonObject frame;
            frame.insert(QStringLiteral("type"), QStringLiteral("frame"));
            frame.insert(QStringLiteral("id"), frameId);
            frame.insert(QStringLiteral("name"), layer.value(QStringLiteral("name")).toString());
            frame.insert(QStringLiteral("x"), bounds.x());
            frame.insert(QStringLiteral("y"), bounds.y());
            frame.insert(QStringLiteral("width"), bounds.width());
            frame.insert(QStringLiteral("height"), bounds.height());
            frame.insert(QStringLiteral("angle"), 0.0);
            frame.insert(QStringLiteral("strokeColor"), QStringLiteral("#000000"));
            frame.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            frame.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
            frame.insert(QStringLiteral("strokeWidth"), 1);
            frame.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
            frame.insert(QStringLiteral("roughness"), 1);
            frame.insert(QStringLiteral("opacity"), 100.0);
            frame.insert(QStringLiteral("groupIds"), QJsonArray());
            frame.insert(QStringLiteral("frameId"), QJsonValue::Null);
            frame.insert(QStringLiteral("roundness"), QJsonValue::Null);
            frame.insert(QStringLiteral("boundElements"), QJsonArray());
            frame.insert(QStringLiteral("version"), 1);
            frame.insert(QStringLiteral("versionNonce"), 1);
            frame.insert(QStringLiteral("created"), 1);
            frame.insert(QStringLiteral("updated"), 1);
            frame.insert(QStringLiteral("seed"), layerId);
            frame.insert(QStringLiteral("index"), QStringLiteral("a%1").arg(900000 + layerId, 6, 10, QLatin1Char('0')));
            frame.insert(QStringLiteral("isDeleted"), false);
            frame.insert(QStringLiteral("link"), QJsonValue::Null);
            frame.insert(QStringLiteral("locked"), false);
            frameElements.append(frame);
            frameIds.insert(layerId, frameId);
        }
    }
    // 需求 15：Excalidraw 表达不了图片填充，含 fillImage 的图元按纯色导出并汇总一条提示。
    int fillImageCount = 0;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const QString kind = item.value(QStringLiteral("kind")).toString();
        const QString imageData = kind == QStringLiteral("shape") ? ImageDataFor(item) : QString();
        const bool asImageElement = !imageData.isEmpty() &&
                                    item.value(QStringLiteral("shapeType")).toString() == QStringLiteral("rectangle");
        if (!asImageElement && item.value(QStringLiteral("fillImage")).isObject()) {
            ++fillImageCount;
        }
        QString degrade;
        QJsonObject element;
        if (kind == QStringLiteral("shape")) {
            const QString shapeType = item.value(QStringLiteral("shapeType")).toString();
            if (asImageElement) {
                // 图片图元：fileId 指向 files 表，元素自身保持矩形框（可旋转、可缩放）。
                element = BaseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
                element.insert(QStringLiteral("type"), QStringLiteral("image"));
                const QString fileId = QStringLiteral("sbimg%1").arg(imageFileIndex, 6, 10, QLatin1Char('0'));
                ++imageFileIndex;
                element.insert(QStringLiteral("fileId"), fileId);
                element.insert(QStringLiteral("status"), QStringLiteral("saved"));
                QJsonArray scale;
                scale.append(1.0);
                scale.append(1.0);
                element.insert(QStringLiteral("scale"), scale);
                element.insert(QStringLiteral("crop"), QJsonValue::Null);
                element.insert(QStringLiteral("strokeColor"), QStringLiteral("transparent"));
                element.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
                element.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
                element.insert(QStringLiteral("strokeWidth"), 1);
                element.insert(QStringLiteral("roundness"), QJsonValue::Null);
                QJsonObject file;
                file.insert(QStringLiteral("id"), fileId);
                file.insert(QStringLiteral("mimeType"), QStringLiteral("image/png"));
                file.insert(QStringLiteral("dataURL"), QStringLiteral("data:image/png;base64,") + imageData);
                file.insert(QStringLiteral("created"), 1);
                file.insert(QStringLiteral("lastRetrieved"), 1);
                filesTable.insert(fileId, file);
            }
            // 菱形已是独立类型，外接框即两条对角线，直接导出 Excalidraw 原生 diamond；
            // 旧的“90° 平行四边形 + 45° 旋转”预置仍按原换算兜底。
            const bool isDiamondPreset =
                shapeType == QStringLiteral("parallelogram") &&
                qFuzzyCompare(item.value(QStringLiteral("parallelogramAngle")).toDouble(60.0), 90.0);
            if (asImageElement) {
                // 已按 image 元素处理，不再走图形分支。
            } else if (shapeType == QStringLiteral("rectangle") || shapeType == QStringLiteral("ellipse") ||
                shapeType == QStringLiteral("diamond") || isDiamondPreset) {
                element = StoryBoardShape(item);
                const QJsonObject base = BaseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
                for (auto it = base.constBegin(); it != base.constEnd(); ++it) {
                    if (!element.contains(it.key())) {
                        element.insert(it.key(), it.value());
                    }
                }
                if (isDiamondPreset) {
                    element.insert(QStringLiteral("type"), QStringLiteral("diamond"));
                    const qreal side = qMax<qreal>(1.0, item.value(QStringLiteral("w")).toDouble());
                    const qreal diag = side * qSqrt(2.0);
                    element.insert(QStringLiteral("width"), diag);
                    element.insert(QStringLiteral("height"), diag);
                    element.insert(QStringLiteral("angle"),
                                   qDegreesToRadians(NumberOr(item, QStringLiteral("rotation"), 0.0) + 45.0));
                } else {
                    element.insert(QStringLiteral("type"), shapeType);
                }
                // 斜纹 / 网格 / 蜂窝等填充样式与透明度 Excalidraw 表达不了，记进 customData 供再导入还原。
                element.insert(QStringLiteral("customData"), StyleCustomData(item));
            } else {
                // 三角形/星形/多边形/自定义形状在 Excalidraw 里没有原生类型，落成矩形并记原类型。
                element = StoryBoardShape(item);
                const QJsonObject base = BaseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
                for (auto it = base.constBegin(); it != base.constEnd(); ++it) {
                    if (!element.contains(it.key())) {
                        element.insert(it.key(), it.value());
                    }
                }
                element.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
                QJsonObject custom = StyleCustomData(item);
                custom.insert(QStringLiteral("storyboardShapeType"), shapeType);
                element.insert(QStringLiteral("customData"), custom);
                degrade = QStringLiteral("%1 导出为矩形").arg(shapeType);
            }
        } else if (kind == QStringLiteral("text")) {
            element = BaseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
            element.insert(QStringLiteral("type"), QStringLiteral("text"));
            element.insert(QStringLiteral("text"), item.value(QStringLiteral("text")).toString());
            element.insert(QStringLiteral("strokeColor"),
                           ColorOr(item, QStringLiteral("color"), QColor(30, 41, 59)).name(QColor::HexRgb));
            element.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            element.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
            element.insert(QStringLiteral("strokeWidth"), 1);
            element.insert(QStringLiteral("fontSize"), NumberOr(item, QStringLiteral("fontSize"), 15.0) * 1.333);
            element.insert(QStringLiteral("fontFamily"),
                           ExcalidrawFontFamily(item.value(QStringLiteral("fontFamily")).toString()));
            element.insert(QStringLiteral("textAlign"), QStringLiteral("left"));
            element.insert(QStringLiteral("verticalAlign"), QStringLiteral("top"));
            element.insert(QStringLiteral("containerId"), QJsonValue::Null);
            element.insert(QStringLiteral("originalText"), item.value(QStringLiteral("text")).toString());
            element.insert(QStringLiteral("lineHeight"), 1.25);
            element.insert(QStringLiteral("autoResize"), true);
        } else if (IsLineKind(kind)) {
            element = BaseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
            const QString startHead = HeadValue(item, QStringLiteral("startArrow"));
            const QString endHead = HeadValue(item, QStringLiteral("endArrow"));
            element.insert(QStringLiteral("type"), (startHead.isEmpty() && endHead.isEmpty())
                                                       ? QStringLiteral("line")
                                                       : QStringLiteral("arrow"));
            element.insert(QStringLiteral("strokeColor"),
                           ColorOr(item, QStringLiteral("color"), QColor(30, 41, 59)).name(QColor::HexRgb));
            element.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            element.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
            element.insert(QStringLiteral("strokeWidth"), NumberOr(item, QStringLiteral("width"), 2.0));
            element.insert(QStringLiteral("startArrowhead"),
                           startHead.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(startHead));
            element.insert(QStringLiteral("endArrowhead"),
                           endHead.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(endHead));
            element.insert(QStringLiteral("startBinding"), QJsonValue::Null);
            element.insert(QStringLiteral("endBinding"), QJsonValue::Null);
            element.insert(QStringLiteral("lastCommittedPoint"), QJsonValue::Null);
            element.insert(QStringLiteral("points"), RelativePoints(item.value(QStringLiteral("segments")).toArray(),
                                                                    NumberOr(item, QStringLiteral("x"), 0.0),
                                                                    NumberOr(item, QStringLiteral("y"), 0.0)));
            const QJsonObject headCustom = HeadCustomData(item);
            if (!headCustom.isEmpty()) {
                element.insert(QStringLiteral("customData"), headCustom);
            }
        } else {
            continue;
        }
        if (!degrade.isEmpty() && warnings) {
            warnings->append(degrade);
        }
        QJsonArray groupIds;
        int group = item.value(QStringLiteral("group")).toInt(0);
        for (int depth = 0; group > 0 && depth < 64; ++depth) {
            groupIds.append(QString::number(group));
            group = groupParents.value(group, 0);
        }
        if (!groupIds.isEmpty()) {
            element.insert(QStringLiteral("groupIds"), groupIds);
        }
        const QString frameId = frameIds.value(item.value(QStringLiteral("layer")).toInt(0));
        if (!frameId.isEmpty()) {
            element.insert(QStringLiteral("frameId"), frameId);
        }
        // 规范字段：index 决定同层级叠放次序，用补零的字母串保证字典序与数值序一致。
        element.insert(QStringLiteral("created"), 1);
        element.insert(QStringLiteral("index"), QStringLiteral("a%1").arg(index, 6, 10, QLatin1Char('0')));
        elements.append(element);
        ++index;
    }
    for (const QJsonValue& frame : frameElements) {
        elements.append(frame);
    }
    if (fillImageCount > 0 && warnings) {
        warnings->append(QStringLiteral("%1 个图元的图片填充导出为纯色").arg(fillImageCount));
    }

    QJsonObject scene;
    scene.insert(QStringLiteral("type"), QLatin1String(SCENE_TYPE));
    scene.insert(QStringLiteral("version"), 2);
    scene.insert(QStringLiteral("source"), QStringLiteral("https://storyboard.local"));
    scene.insert(QStringLiteral("elements"), elements);
    scene.insert(QStringLiteral("files"), filesTable);

    const QJsonObject canvas = document.value(QStringLiteral("canvas")).toObject();
    QJsonObject appState;
    appState.insert(QStringLiteral("gridSize"), 64);
    appState.insert(QStringLiteral("viewBackgroundColor"), document.value(QStringLiteral("background"))
                                                               .toObject()
                                                               .value(QStringLiteral("color"))
                                                               .toString(QStringLiteral("#ffffff")));
    appState.insert(QStringLiteral("scrollX"), 0);
    appState.insert(QStringLiteral("scrollY"), 0);
    QJsonObject zoom;
    zoom.insert(QStringLiteral("value"), 1.0);
    appState.insert(QStringLiteral("zoom"), zoom);
    appState.insert(QStringLiteral("viewportWidth"), canvas.value(QStringLiteral("width")).toDouble(1536.0));
    appState.insert(QStringLiteral("viewportHeight"), canvas.value(QStringLiteral("height")).toDouble(1024.0));
    scene.insert(QStringLiteral("appState"), appState);
    return scene;
}

QJsonObject StoryBoardToClipboard(const QJsonObject& document, QStringList* warnings)
{
    const QJsonObject scene = StoryBoardToScene(document, warnings);
    QJsonObject clipboard;
    clipboard.insert(QStringLiteral("type"), QString::fromLatin1(CLIPBOARD_TYPE));
    clipboard.insert(QStringLiteral("elements"), scene.value(QStringLiteral("elements")));
    clipboard.insert(QStringLiteral("files"), scene.value(QStringLiteral("files")));
    return clipboard;
}

} // namespace Excalidraw
