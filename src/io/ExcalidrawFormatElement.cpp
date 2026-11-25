#include "io/ExcalidrawFormatInternal.h"

#include "items/properties/TextAttributes.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QtMath>
#include <utility>

namespace ExcalidrawFormatDetail {

// 单条折线最多保留的点数：再多对画板没有意义，只会把曲线图元撑爆。
constexpr int MAX_POINTS = 10000;

// segments 在画板里是“相对图元原点”的本地坐标，而 Excalidraw 的点是世界坐标；
// origin 传包围盒左上角（= 图元 x/y），否则整条线会再平移一次、落到画布外。
QJsonArray PolylineSegments(const QList<QPointF>& points, const QPointF& origin)
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

} // namespace ExcalidrawFormatDetail
