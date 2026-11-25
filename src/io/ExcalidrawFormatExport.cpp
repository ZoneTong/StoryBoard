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
    // 容器文字导出时要回到容器的框位，先按 uid 建一份图元索引。
    QHash<QString, QJsonObject> itemsByUid;
    for (const QJsonValue& itemValue : items) {
        const QJsonObject item = itemValue.toObject();
        const QString uid = item.value(QStringLiteral("uid")).toString();
        if (!uid.isEmpty()) {
            itemsByUid.insert(uid, item);
        }
    }
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
            const QString containerUid = item.value(QStringLiteral("containerId")).toString();
            if (!containerUid.isEmpty()) {
                element.insert(QStringLiteral("containerId"), containerUid);
                element.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
                element.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));
                element.insert(QStringLiteral("autoResize"), false);
                const QJsonObject container = itemsByUid.value(containerUid);
                if (!container.isEmpty()) {
                    element.insert(QStringLiteral("x"), NumberOr(container, QStringLiteral("x"), 0.0));
                    element.insert(QStringLiteral("y"), NumberOr(container, QStringLiteral("y"), 0.0));
                    element.insert(QStringLiteral("width"), NumberOr(container, QStringLiteral("w"), 0.0));
                    element.insert(QStringLiteral("height"), NumberOr(container, QStringLiteral("h"), 0.0));
                }
            } else {
                element.insert(QStringLiteral("containerId"), QJsonValue::Null);
                element.insert(QStringLiteral("textAlign"), QStringLiteral("left"));
                element.insert(QStringLiteral("verticalAlign"), QStringLiteral("top"));
                element.insert(QStringLiteral("autoResize"), true);
            }
            element.insert(QStringLiteral("originalText"), item.value(QStringLiteral("text")).toString());
            element.insert(QStringLiteral("lineHeight"), 1.25);
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
            const QJsonObject startBinding = item.value(QStringLiteral("startBinding")).toObject();
            const QJsonObject endBinding = item.value(QStringLiteral("endBinding")).toObject();
            element.insert(QStringLiteral("startBinding"),
                           startBinding.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(startBinding));
            element.insert(QStringLiteral("endBinding"),
                           endBinding.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(endBinding));
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
    // 反向关系：被绑定的图形要在 boundElements 里列出箭头 / 容器文字，
    // 否则 Excalidraw 打开后知道箭头绑了谁，却不知道图形上挂了什么。
    {
        QHash<QString, int> elementIndexById;
        for (int i = 0; i < elements.size(); ++i) {
            const QString id = elements.at(i).toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && !elementIndexById.contains(id)) {
                elementIndexById.insert(id, i);
            }
        }
        auto AppendBound = [&elements, &elementIndexById](const QString& targetId, const QString& boundId,
                                                          const QString& type) {
            const int targetIndex = elementIndexById.value(targetId, -1);
            if (targetIndex < 0 || boundId.isEmpty()) {
                return;
            }
            QJsonObject target = elements.at(targetIndex).toObject();
            QJsonArray bound = target.value(QStringLiteral("boundElements")).toArray();
            for (const QJsonValue& value : bound) {
                if (value.toObject().value(QStringLiteral("id")).toString() == boundId) {
                    return;
                }
            }
            QJsonObject reference;
            reference.insert(QStringLiteral("id"), boundId);
            reference.insert(QStringLiteral("type"), type);
            bound.append(reference);
            target.insert(QStringLiteral("boundElements"), bound);
            elements.replace(targetIndex, target);
        };
        for (const QJsonValue& value : elements) {
            const QJsonObject element = value.toObject();
            const QString boundId = element.value(QStringLiteral("id")).toString();
            for (const QString& key : {QStringLiteral("startBinding"), QStringLiteral("endBinding")}) {
                const QString targetId =
                    element.value(key).toObject().value(QStringLiteral("elementId")).toString();
                if (!targetId.isEmpty()) {
                    AppendBound(targetId, boundId, QStringLiteral("arrow"));
                }
            }
            const QString containerId = element.value(QStringLiteral("containerId")).toString();
            if (!containerId.isEmpty()) {
                AppendBound(containerId, boundId, QStringLiteral("text"));
            }
        }
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
