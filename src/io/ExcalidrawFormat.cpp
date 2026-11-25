#include "io/ExcalidrawFormat.h"

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

// 需求 9：Excalidraw 的 head 样式映射到画板箭头；空心三角画板有，直接保留。
QString HeadStyle(const QJsonValue& value)
{
    const QString type = value.isString() ? value.toString() : QString();
    if (type == QStringLiteral("bar")) {
        return QStringLiteral("bar");
    }
    if (type == QStringLiteral("dot")) {
        return QStringLiteral("dot");
    }
    if (type == QStringLiteral("triangle")) {
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

QJsonArray PolylineSegments(const QList<QPointF>& points)
{
    QJsonArray segments;
    const int last = qMin(points.size() - 1, MAX_POINTS - 1);
    for (int i = 1; i <= last; ++i) {
        const QPointF p0 = points.at(i - 1);
        const QPointF p1 = points.at(i);
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

QRectF BoundsOf(const QList<QPointF>& points)
{
    QRectF bounds;
    for (const QPointF& point : points) {
        bounds = bounds.isNull() ? QRectF(point, QSizeF(0, 0)) : bounds.united(QRectF(point, QSizeF(0, 0)));
    }
    return bounds;
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
        QJsonObject item = ItemBase(QStringLiteral("image"), bounds, z, layer);
        item.insert(QStringLiteral("name"), element.value(QStringLiteral("id")).toString());
        item.insert(QStringLiteral("image"), dataUrl.mid(comma + 1));
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
            // 原填充样式（斜纹 / 网格 / 蜂窝等）在 customData 里，取不到才降级为实心。
            const QString restoredStyle = CustomText(element, QStringLiteral("fillStyle"));
            if (!restoredStyle.isEmpty()) {
                fillStyle = restoredStyle;
            } else {
                const QString sourceStyle = element.value(QStringLiteral("fillStyle")).toString();
                fillStyle = QStringLiteral("solid");
                if (!sourceStyle.isEmpty() && sourceStyle != QStringLiteral("solid") && warnings) {
                    warnings->append(QStringLiteral("fillStyle %1 降级为实心填充").arg(sourceStyle));
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
        item.insert(QStringLiteral("strokeStyle"), 1);
        item.insert(QStringLiteral("opacity"), NumberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
        item.insert(QStringLiteral("cornerRadius"),
                    qMax<qreal>(0.0, NumberOr(element.value(QStringLiteral("roundness")).toObject(),
                                              QStringLiteral("value"), 0.0)));
        item.insert(QStringLiteral("rotation"), qRadiansToDegrees(NumberOr(element, QStringLiteral("angle"), 0.0)));
        return item;
    }

    if (type == QStringLiteral("text")) {
        QJsonObject item = ItemBase(QStringLiteral("text"), bounds, z, layer);
        item.insert(QStringLiteral("text"), element.value(QStringLiteral("text")).toString());
        item.insert(QStringLiteral("color"),
                    ColorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexArgb));
        // 需求 9：Excalidraw 字体用 px，画板用 pt，约 1.333 倍换算。
        item.insert(QStringLiteral("fontSize"),
                    qMax<qreal>(6.0, NumberOr(element, QStringLiteral("fontSize"), 20.0) / 1.333));
        item.insert(QStringLiteral("baseWidth"), bounds.width());
        item.insert(QStringLiteral("baseHeight"), bounds.height());
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
        item.insert(QStringLiteral("strokeStyle"), 1);
        item.insert(QStringLiteral("baseWidth"), lineBounds.width());
        item.insert(QStringLiteral("baseHeight"), lineBounds.height());
        item.insert(QStringLiteral("dot"), false);
        item.insert(QStringLiteral("linear"), true);
        // 类型标识只写一次：手绘笔迹为 freehand，其余折线为 straightline。
        item.insert(QStringLiteral("startArrow"), ImportHeadStyle(element, QStringLiteral("startArrow")));
        item.insert(QStringLiteral("endArrow"), ImportHeadStyle(element, QStringLiteral("endArrow")));
        item.insert(QStringLiteral("segments"), PolylineSegments(points));
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
                   item.value(QStringLiteral("fillStyle")).toString() == QStringLiteral("solid")
                       ? QStringLiteral("solid")
                       : QStringLiteral("hachure"));
    element.insert(QStringLiteral("strokeWidth"), NumberOr(item, QStringLiteral("strokeWidth"), 2.0));
    element.insert(QStringLiteral("roundness"), QJsonObject{});
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
    element.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
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
                           const QJsonObject& files, bool keepImages)
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
        const int layer = frameLayer.value(element.value(QStringLiteral("frameId")).toString(), 1);
        const QJsonObject item = ElementToItem(element, z, layer, warnings, files, keepImages);
        if (item.isEmpty()) {
            continue;
        }
        items.append(item);
        ++z;
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
    const QJsonArray items = ElementsToItems(scene.value(QStringLiteral("elements")).toArray(), warnings, &layers,
                                             scene.value(QStringLiteral("files")).toObject(), keepImages);
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
    document.insert(QStringLiteral("activeLayer"), 1);
    document.insert(QStringLiteral("items"), items);
    return document;
}

QJsonObject StoryBoardToScene(const QJsonObject& document, QStringList* warnings)
{
    const QJsonArray items = document.value(QStringLiteral("items")).toArray();
    QJsonArray elements;
    int index = 0;
    // 需求 15：Excalidraw 表达不了图片填充，含 fillImage 的图元按纯色导出并汇总一条提示。
    int fillImageCount = 0;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const QString kind = item.value(QStringLiteral("kind")).toString();
        if (item.value(QStringLiteral("fillImage")).isObject()) {
            ++fillImageCount;
        }
        QString degrade;
        QJsonObject element;
        if (kind == QStringLiteral("shape")) {
            const QString shapeType = item.value(QStringLiteral("shapeType")).toString();
            // 菱形已是独立类型，外接框即两条对角线，直接导出 Excalidraw 原生 diamond；
            // 旧的“90° 平行四边形 + 45° 旋转”预置仍按原换算兜底。
            const bool isDiamondPreset =
                shapeType == QStringLiteral("parallelogram") &&
                qFuzzyCompare(item.value(QStringLiteral("parallelogramAngle")).toDouble(60.0), 90.0);
            if (shapeType == QStringLiteral("rectangle") || shapeType == QStringLiteral("ellipse") ||
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
            element.insert(QStringLiteral("fontFamily"), 1);
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
        elements.append(element);
        ++index;
    }
    if (fillImageCount > 0 && warnings) {
        warnings->append(QStringLiteral("%1 个图元的图片填充导出为纯色").arg(fillImageCount));
    }

    QJsonObject scene;
    scene.insert(QStringLiteral("type"), QLatin1String(SCENE_TYPE));
    scene.insert(QStringLiteral("version"), 2);
    scene.insert(QStringLiteral("source"), QStringLiteral("https://storyboard.local"));
    scene.insert(QStringLiteral("elements"), elements);
    scene.insert(QStringLiteral("files"), QJsonObject{});

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

} // namespace Excalidraw
