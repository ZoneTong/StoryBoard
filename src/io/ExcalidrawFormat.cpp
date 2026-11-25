#include "io/ExcalidrawFormat.h"

#include "io/ExcalidrawFormatInternal.h"

#include <QColor>
#include <QHash>
#include <QJsonDocument>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QtMath>

using namespace ExcalidrawFormatDetail;

namespace {
// 元素 id 优先沿用图元的稳定 uid，导入→导出才能指回同一个对象；旧文档没有 uid 时按序生成。
QString ElementId(const QJsonObject& item, int index)
{
    const QString uid = item.value(QStringLiteral("uid")).toString();
    return uid.isEmpty() ? QStringLiteral("sb%1").arg(index, 6, 10, QLatin1Char('0')) : uid;
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
    QSet<QString> usedUids;
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
        // 图元身份沿用 Excalidraw 的元素 id，后续绑定/容器关系才能指回同一个对象。
        const QString sourceUid = element.value(QStringLiteral("id")).toString().trimmed();
        if (!sourceUid.isEmpty() && !usedUids.contains(sourceUid)) {
            item.insert(QStringLiteral("uid"), sourceUid);
            usedUids.insert(sourceUid);
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
                element = BaseElement(item, ElementId(item, index));
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
                const QJsonObject base = BaseElement(item, ElementId(item, index));
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
                const QJsonObject base = BaseElement(item, ElementId(item, index));
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
            element = BaseElement(item, ElementId(item, index));
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
            element = BaseElement(item, ElementId(item, index));
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
