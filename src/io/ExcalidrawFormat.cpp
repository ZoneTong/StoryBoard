#include "io/ExcalidrawFormat.h"

#include "io/ExcalidrawFormatInternal.h"
#include "items/text/TextItem.h"

#include <QColor>
#include <QHash>
#include <QJsonDocument>
#include <QRectF>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QtMath>
#include <utility>

using namespace ExcalidrawFormatDetail;


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
        // Excalidraw 元素级附加信息原样留在 native JSON；frameId 已经换算成 layer。
        const QString link = element.value(QStringLiteral("link")).toString().trimmed();
        if (!link.isEmpty()) {
            item.insert(QStringLiteral("link"), link);
        }
        if (element.value(QStringLiteral("locked")).toBool(false)) {
            item.insert(QStringLiteral("locked"), true);
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

    // 容器文字：导入与粘贴都按容器宽度重新折行并回到容器中心，避免沿 Excalidraw
    // 的单行宽度截断。粘贴走 ElementsToItems，因此这段必须放在这里而不是场景导入。
    {
        QHash<QString, QJsonObject> byUid;
        for (const QJsonValue& value : std::as_const(items)) {
            const QJsonObject item = value.toObject();
            const QString uid = item.value(QStringLiteral("uid")).toString();
            if (!uid.isEmpty()) {
                byUid.insert(uid, item);
            }
        }
        for (int i = 0; i < items.size(); ++i) {
            QJsonObject item = items.at(i).toObject();
            const QString containerId = item.value(QStringLiteral("containerId")).toString();
            const QJsonObject container = byUid.value(containerId);
            if (containerId.isEmpty() || container.isEmpty()) {
                continue;
            }
            TextItem probe;
            probe.FromJson(item);
            const QSizeF size = probe.ContainerLayoutSize(container.value(QStringLiteral("w")).toDouble());
            item.insert(QStringLiteral("x"), container.value(QStringLiteral("x")).toDouble());
            item.insert(QStringLiteral("y"),
                        container.value(QStringLiteral("y")).toDouble() +
                            (container.value(QStringLiteral("h")).toDouble() - size.height()) / 2.0);
            item.insert(QStringLiteral("w"), size.width());
            item.insert(QStringLiteral("h"), size.height());
            items.replace(i, item);
        }
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
    QJsonArray items = ElementsToItems(scene.value(QStringLiteral("elements")).toArray(), warnings, &layers,
                                       scene.value(QStringLiteral("files")).toObject(), keepImages, &groupParents);
    if (items.isEmpty()) {
        return {};
    }

    // 画布取所有元素的包围盒并留边；负坐标元素不能落在画布外，原点跟着往负方向挪。
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        const qreal x = item.value(QStringLiteral("x")).toDouble();
        const qreal y = item.value(QStringLiteral("y")).toDouble();
        const qreal w = item.value(QStringLiteral("w")).toDouble();
        const qreal h = item.value(QStringLiteral("h")).toDouble();
        left = qMin(left, x);
        top = qMin(top, y);
        right = qMax(right, x + w);
        bottom = qMax(bottom, y + h);
    }

    QJsonObject document;
    document.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    document.insert(QStringLiteral("version"), 2);
    document.insert(QStringLiteral("generator"), QStringLiteral("StoryBoard Excalidraw import"));
    const qreal originX = left < 0.0 ? left - 64.0 : 0.0;
    const qreal originY = top < 0.0 ? top - 64.0 : 0.0;
    QJsonObject canvas;
    canvas.insert(QStringLiteral("x"), originX);
    canvas.insert(QStringLiteral("y"), originY);
    canvas.insert(QStringLiteral("width"), qMax<qreal>(512.0, right - originX + 64.0));
    canvas.insert(QStringLiteral("height"), qMax<qreal>(512.0, bottom - originY + 64.0));
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

} // namespace Excalidraw
