#include "ExcalidrawFormat.h"

#include <QColor>
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

qreal numberOr(const QJsonObject& object, const QString& key, qreal fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toDouble() : fallback;
}

QColor colorOr(const QJsonObject& object, const QString& key, const QColor& fallback)
{
    const QString text = object.value(key).toString();
    if (text.isEmpty() || text == QStringLiteral("transparent")) {
        return QColor(Qt::transparent);
    }
    const QColor color(text);
    return color.isValid() ? color : fallback;
}

// 需求 9：Excalidraw 的 head 样式映射到画板箭头；空心三角画板有，直接保留。
QString headStyle(const QJsonValue& value)
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

QJsonObject itemBase(const QString& type, const QRectF& bounds, int z)
{
    QJsonObject item;
    item.insert(QStringLiteral("type"), type);
    item.insert(QStringLiteral("x"), bounds.x());
    item.insert(QStringLiteral("y"), bounds.y());
    item.insert(QStringLiteral("w"), bounds.width());
    item.insert(QStringLiteral("h"), bounds.height());
    item.insert(QStringLiteral("layer"), 1);
    item.insert(QStringLiteral("height"), 1);
    item.insert(QStringLiteral("z"), z);
    return item;
}

QJsonArray polylineSegments(const QList<QPointF>& points)
{
    QJsonArray segments;
    for (int i = 1; i < points.size(); ++i) {
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

QList<QPointF> absolutePoints(const QJsonObject& element)
{
    QList<QPointF> points;
    const QJsonArray raw = element.value(QStringLiteral("points")).toArray();
    for (const QJsonValue& value : raw) {
        const QJsonArray pair = value.toArray();
        if (pair.size() < 2) {
            continue;
        }
        points.append(QPointF(pair.at(0).toDouble() + numberOr(element, QStringLiteral("x"), 0.0),
                              pair.at(1).toDouble() + numberOr(element, QStringLiteral("y"), 0.0)));
    }
    return points;
}

QRectF boundsOf(const QList<QPointF>& points)
{
    QRectF bounds;
    for (const QPointF& point : points) {
        bounds = bounds.isNull() ? QRectF(point, QSizeF(0, 0)) : bounds.united(QRectF(point, QSizeF(0, 0)));
    }
    return bounds;
}

QJsonObject elementToItem(const QJsonObject& element, int z, QStringList* warnings)
{
    const QString type = element.value(QStringLiteral("type")).toString();
    const QRectF bounds(numberOr(element, QStringLiteral("x"), 0.0), numberOr(element, QStringLiteral("y"), 0.0),
                        numberOr(element, QStringLiteral("width"), 0.0),
                        numberOr(element, QStringLiteral("height"), 0.0));

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("diamond")) {
        QJsonObject item = itemBase(QStringLiteral("shape"), bounds, z);
        item.insert(
            QStringLiteral("shapeType"),
            type == QStringLiteral("ellipse")
                ? QStringLiteral("ellipse")
                : (type == QStringLiteral("diamond") ? QStringLiteral("diamond") : QStringLiteral("rectangle")));
        const QColor stroke = colorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59));
        QColor fill = colorOr(element, QStringLiteral("backgroundColor"), QColor(Qt::transparent));
        QString fillStyle = QStringLiteral("none");
        if (fill.alpha() > 0) {
            const QString sourceStyle = element.value(QStringLiteral("fillStyle")).toString();
            fillStyle = QStringLiteral("solid");
            if (!sourceStyle.isEmpty() && sourceStyle != QStringLiteral("solid") && warnings) {
                warnings->append(QStringLiteral("fillStyle %1 降级为实心填充").arg(sourceStyle));
            }
        }
        item.insert(QStringLiteral("strokeColor"), stroke.name(QColor::HexArgb));
        item.insert(QStringLiteral("fillColor"), fill.name(QColor::HexArgb));
        item.insert(QStringLiteral("fillStyle"), fillStyle);
        item.insert(QStringLiteral("strokeWidth"),
                    qMax<qreal>(0.5, numberOr(element, QStringLiteral("strokeWidth"), 2.0)));
        item.insert(QStringLiteral("strokeStyle"), 1);
        item.insert(QStringLiteral("opacity"), numberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
        item.insert(QStringLiteral("cornerRadius"),
                    qMax<qreal>(0.0, numberOr(element.value(QStringLiteral("roundness")).toObject(),
                                              QStringLiteral("value"), 0.0)));
        item.insert(QStringLiteral("rotation"), qRadiansToDegrees(numberOr(element, QStringLiteral("angle"), 0.0)));
        return item;
    }

    if (type == QStringLiteral("text")) {
        QJsonObject item = itemBase(QStringLiteral("text"), bounds, z);
        item.insert(QStringLiteral("text"), element.value(QStringLiteral("text")).toString());
        item.insert(QStringLiteral("color"),
                    colorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexArgb));
        // 需求 9：Excalidraw 字体用 px，画板用 pt，约 1.333 倍换算。
        item.insert(QStringLiteral("fontSize"),
                    qMax<qreal>(6.0, numberOr(element, QStringLiteral("fontSize"), 20.0) / 1.333));
        item.insert(QStringLiteral("baseWidth"), bounds.width());
        item.insert(QStringLiteral("baseHeight"), bounds.height());
        item.insert(QStringLiteral("rotation"), qRadiansToDegrees(numberOr(element, QStringLiteral("angle"), 0.0)));
        return item;
    }

    if (type == QStringLiteral("line") || type == QStringLiteral("arrow") || type == QStringLiteral("freedraw")) {
        const QList<QPointF> points = absolutePoints(element);
        if (points.size() < 2) {
            return {};
        }
        if (points.size() == 2 && type != QStringLiteral("freedraw")) {
            // 两点直线/箭头：用画板直线图元，两端箭头按 Excalidraw 的 head 映射。
            const QPointF start = points.constFirst();
            const QPointF end = points.constLast();
            QJsonObject item = itemBase(QStringLiteral("shape"),
                                        QRectF(QPointF(qMin(start.x(), end.x()), qMin(start.y(), end.y())),
                                               QSizeF(qAbs(end.x() - start.x()), qAbs(end.y() - start.y()))),
                                        z);
            item.insert(QStringLiteral("shapeType"), QStringLiteral("line"));
            item.insert(QStringLiteral("strokeColor"),
                        colorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexArgb));
            item.insert(QStringLiteral("fillColor"), QStringLiteral("#00000000"));
            item.insert(QStringLiteral("fillStyle"), QStringLiteral("none"));
            item.insert(QStringLiteral("strokeWidth"),
                        qMax<qreal>(0.5, numberOr(element, QStringLiteral("strokeWidth"), 2.0)));
            item.insert(QStringLiteral("strokeStyle"), 1);
            item.insert(QStringLiteral("opacity"), numberOr(element, QStringLiteral("opacity"), 100.0) / 100.0);
            item.insert(QStringLiteral("startArrow"), headStyle(element.value(QStringLiteral("startArrowhead"))));
            item.insert(QStringLiteral("endArrow"), headStyle(element.value(QStringLiteral("endArrowhead"))));
            return item;
        }
        // 多点折线：保留拐点与两端箭头，落到曲线图元。
        const QRectF lineBounds = boundsOf(points);
        QJsonObject item = itemBase(QStringLiteral("curve"), lineBounds, z);
        item.insert(QStringLiteral("color"),
                    colorOr(element, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexArgb));
        item.insert(QStringLiteral("width"), qMax<qreal>(0.5, numberOr(element, QStringLiteral("strokeWidth"), 2.0)));
        item.insert(QStringLiteral("strokeStyle"), 1);
        item.insert(QStringLiteral("baseWidth"), lineBounds.width());
        item.insert(QStringLiteral("baseHeight"), lineBounds.height());
        item.insert(QStringLiteral("dot"), false);
        item.insert(QStringLiteral("linear"), true);
        item.insert(QStringLiteral("startArrow"), headStyle(element.value(QStringLiteral("startArrowhead"))));
        item.insert(QStringLiteral("endArrow"), headStyle(element.value(QStringLiteral("endArrowhead"))));
        item.insert(QStringLiteral("segments"), polylineSegments(points));
        return item;
    }

    if (warnings && type != QStringLiteral("frame")) {
        warnings->append(QStringLiteral("跳过不支持的 Excalidraw 元素类型 %1").arg(type));
    }
    return {};
}

QJsonObject storyBoardShape(const QJsonObject& item)
{
    const QString shapeType = item.value(QStringLiteral("shapeType")).toString();
    const QString type =
        shapeType == QStringLiteral("ellipse")
            ? QStringLiteral("ellipse")
            : (shapeType == QStringLiteral("diamond") ? QStringLiteral("diamond") : QStringLiteral("rectangle"));
    QJsonObject element;
    element.insert(QStringLiteral("type"), type);
    element.insert(QStringLiteral("strokeColor"),
                   colorOr(item, QStringLiteral("strokeColor"), QColor(30, 41, 59)).name(QColor::HexRgb));
    const QColor fill = colorOr(item, QStringLiteral("fillColor"), QColor(Qt::transparent));
    element.insert(QStringLiteral("backgroundColor"),
                   fill.alpha() == 0 ? QStringLiteral("transparent") : fill.name(QColor::HexRgb));
    element.insert(QStringLiteral("fillStyle"),
                   item.value(QStringLiteral("fillStyle")).toString() == QStringLiteral("solid")
                       ? QStringLiteral("solid")
                       : QStringLiteral("hachure"));
    element.insert(QStringLiteral("strokeWidth"), numberOr(item, QStringLiteral("strokeWidth"), 2.0));
    element.insert(QStringLiteral("roundness"), QJsonObject{});
    return element;
}

QJsonObject baseElement(const QJsonObject& item, const QString& id)
{
    QJsonObject element;
    element.insert(QStringLiteral("id"), id);
    element.insert(QStringLiteral("x"), numberOr(item, QStringLiteral("x"), 0.0));
    element.insert(QStringLiteral("y"), numberOr(item, QStringLiteral("y"), 0.0));
    element.insert(QStringLiteral("width"), numberOr(item, QStringLiteral("w"), 0.0));
    element.insert(QStringLiteral("height"), numberOr(item, QStringLiteral("h"), 0.0));
    element.insert(QStringLiteral("angle"), qDegreesToRadians(numberOr(item, QStringLiteral("rotation"), 0.0)));
    element.insert(QStringLiteral("opacity"), numberOr(item, QStringLiteral("opacity"), 1.0) * 100.0);
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

QString headValue(const QJsonObject& item, const QString& key)
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

QJsonArray relativePoints(const QJsonArray& segments, qreal originX, qreal originY)
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

bool isScene(const QJsonObject& root)
{
    return root.value(QStringLiteral("type")).toString() == QLatin1String(SceneType) &&
           root.value(QStringLiteral("elements")).isArray();
}

bool isClipboard(const QJsonObject& root)
{
    return root.value(QStringLiteral("type")).toString() == QLatin1String(ClipboardType) &&
           root.value(QStringLiteral("elements")).isArray();
}

QJsonArray elementsToItems(const QJsonArray& elements, QStringList* warnings)
{
    QJsonArray items;
    int z = 1000001;
    // 需求 2：导入闸门。元素数超上限时只取前 20000 个，避免恶意或超大场景卡死界面。
    constexpr int kMaxElements = 20000;
    if (elements.size() > kMaxElements) {
        if (warnings) {
            warnings->append(QStringLiteral("Excalidraw 元素过多，只导入前 %1 个（共 %2 个）")
                                 .arg(kMaxElements)
                                 .arg(elements.size()));
        }
    }
    for (const QJsonValue& value : elements) {
        if (items.size() >= kMaxElements) {
            break;
        }
        const QJsonObject element = value.toObject();
        if (element.value(QStringLiteral("isDeleted")).toBool(false)) {
            continue;
        }
        const QJsonObject item = elementToItem(element, z, warnings);
        if (item.isEmpty()) {
            continue;
        }
        items.append(item);
        ++z;
    }
    return items;
}

QJsonObject sceneToStoryBoard(const QJsonObject& scene, QStringList* warnings)
{
    const QJsonArray items = elementsToItems(scene.value(QStringLiteral("elements")).toArray(), warnings);
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

    QJsonObject layer;
    layer.insert(QStringLiteral("id"), 1);
    layer.insert(QStringLiteral("name"), QStringLiteral("图层 1"));
    layer.insert(QStringLiteral("visible"), true);
    layer.insert(QStringLiteral("locked"), false);
    document.insert(QStringLiteral("layers"), QJsonArray{layer});
    document.insert(QStringLiteral("activeLayer"), 1);
    document.insert(QStringLiteral("items"), items);
    return document;
}

QJsonObject storyBoardToScene(const QJsonObject& document, QStringList* warnings)
{
    const QJsonArray items = document.value(QStringLiteral("items")).toArray();
    QJsonArray elements;
    int index = 0;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const QString kind = item.value(QStringLiteral("type")).toString();
        QString degrade;
        QJsonObject element;
        if (kind == QStringLiteral("shape")) {
            const QString shapeType = item.value(QStringLiteral("shapeType")).toString();
            if (shapeType == QStringLiteral("line")) {
                element = baseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
                element.insert(QStringLiteral("type"), headValue(item, QStringLiteral("endArrow")).isEmpty() &&
                                                               headValue(item, QStringLiteral("startArrow")).isEmpty()
                                                           ? QStringLiteral("line")
                                                           : QStringLiteral("arrow"));
                const QString startHead = headValue(item, QStringLiteral("startArrow"));
                const QString endHead = headValue(item, QStringLiteral("endArrow"));
                element.insert(QStringLiteral("startArrowhead"),
                               startHead.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(startHead));
                element.insert(QStringLiteral("endArrowhead"),
                               endHead.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(endHead));
                element.insert(QStringLiteral("lastCommittedPoint"), QJsonValue::Null);
                element.insert(QStringLiteral("startBinding"), QJsonValue::Null);
                element.insert(QStringLiteral("endBinding"), QJsonValue::Null);
                QJsonArray points;
                points.append(QJsonArray{0.0, 0.0});
                points.append(
                    QJsonArray{item.value(QStringLiteral("w")).toDouble(), item.value(QStringLiteral("h")).toDouble()});
                element.insert(QStringLiteral("points"), points);
            } else if (shapeType == QStringLiteral("rectangle") || shapeType == QStringLiteral("ellipse") ||
                       shapeType == QStringLiteral("diamond")) {
                element = storyBoardShape(item);
                const QJsonObject base = baseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
                for (auto it = base.constBegin(); it != base.constEnd(); ++it) {
                    if (!element.contains(it.key())) {
                        element.insert(it.key(), it.value());
                    }
                }
                element.insert(QStringLiteral("type"), shapeType);
            } else {
                // 三角形/星形/多边形/自定义形状在 Excalidraw 里没有原生类型，落成矩形并记原类型。
                element = storyBoardShape(item);
                const QJsonObject base = baseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
                for (auto it = base.constBegin(); it != base.constEnd(); ++it) {
                    if (!element.contains(it.key())) {
                        element.insert(it.key(), it.value());
                    }
                }
                element.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
                QJsonObject custom;
                custom.insert(QStringLiteral("storyboardShapeType"), shapeType);
                element.insert(QStringLiteral("customData"), custom);
                degrade = QStringLiteral("%1 导出为矩形").arg(shapeType);
            }
        } else if (kind == QStringLiteral("text")) {
            element = baseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
            element.insert(QStringLiteral("type"), QStringLiteral("text"));
            element.insert(QStringLiteral("text"), item.value(QStringLiteral("text")).toString());
            element.insert(QStringLiteral("strokeColor"),
                           colorOr(item, QStringLiteral("color"), QColor(30, 41, 59)).name(QColor::HexRgb));
            element.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            element.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
            element.insert(QStringLiteral("strokeWidth"), 1);
            element.insert(QStringLiteral("fontSize"), numberOr(item, QStringLiteral("fontSize"), 15.0) * 1.333);
            element.insert(QStringLiteral("fontFamily"), 1);
            element.insert(QStringLiteral("textAlign"), QStringLiteral("left"));
            element.insert(QStringLiteral("verticalAlign"), QStringLiteral("top"));
            element.insert(QStringLiteral("containerId"), QJsonValue::Null);
            element.insert(QStringLiteral("originalText"), item.value(QStringLiteral("text")).toString());
            element.insert(QStringLiteral("lineHeight"), 1.25);
            element.insert(QStringLiteral("autoResize"), true);
        } else if (kind == QStringLiteral("curve")) {
            element = baseElement(item, QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')));
            const QString startHead = headValue(item, QStringLiteral("startArrow"));
            const QString endHead = headValue(item, QStringLiteral("endArrow"));
            element.insert(QStringLiteral("type"), (startHead.isEmpty() && endHead.isEmpty())
                                                       ? QStringLiteral("line")
                                                       : QStringLiteral("arrow"));
            element.insert(QStringLiteral("strokeColor"),
                           colorOr(item, QStringLiteral("color"), QColor(30, 41, 59)).name(QColor::HexRgb));
            element.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            element.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
            element.insert(QStringLiteral("strokeWidth"), numberOr(item, QStringLiteral("width"), 2.0));
            element.insert(QStringLiteral("startArrowhead"),
                           startHead.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(startHead));
            element.insert(QStringLiteral("endArrowhead"),
                           endHead.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(endHead));
            element.insert(QStringLiteral("startBinding"), QJsonValue::Null);
            element.insert(QStringLiteral("endBinding"), QJsonValue::Null);
            element.insert(QStringLiteral("lastCommittedPoint"), QJsonValue::Null);
            element.insert(QStringLiteral("points"), relativePoints(item.value(QStringLiteral("segments")).toArray(),
                                                                    numberOr(item, QStringLiteral("x"), 0.0),
                                                                    numberOr(item, QStringLiteral("y"), 0.0)));
        } else {
            continue;
        }
        if (!degrade.isEmpty() && warnings) {
            warnings->append(degrade);
        }
        elements.append(element);
        ++index;
    }

    QJsonObject scene;
    scene.insert(QStringLiteral("type"), QLatin1String(SceneType));
    scene.insert(QStringLiteral("version"), 2);
    scene.insert(QStringLiteral("source"), QStringLiteral("https://storyboard.local"));
    scene.insert(QStringLiteral("elements"), elements);
    scene.insert(QStringLiteral("files"), QJsonObject{});

    const QJsonObject canvas = document.value(QStringLiteral("canvas")).toObject();
    QJsonObject appState;
    appState.insert(QStringLiteral("gridSize"), 32);
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
