#include "CanvasItem.h"
#include "CanvasScene.h"
#include "scene/SceneConstants.h"

#include <QBuffer>
#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>
#include <QSet>
#include <algorithm>

namespace {

QString encodePixmap(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return QString();
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString::fromLatin1(bytes.toBase64());
}

QPixmap decodePixmap(const QString& base64)
{
    const QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());
    QPixmap pixmap;
    if (!bytes.isEmpty()) {
        pixmap.loadFromData(bytes, "PNG");
    }
    return pixmap;
}

} // namespace

QJsonObject CanvasScene::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("generator"), QStringLiteral("StoryBoard 0.2"));

    QJsonObject canvas;
    canvas.insert(QStringLiteral("x"), m_canvasOrigin.x());
    canvas.insert(QStringLiteral("y"), m_canvasOrigin.y());
    canvas.insert(QStringLiteral("width"), m_canvasSize.width());
    canvas.insert(QStringLiteral("height"), m_canvasSize.height());
    root.insert(QStringLiteral("canvas"), canvas);

    QJsonObject background;
    switch (m_background.type) {
    case BackgroundType::None:
        background.insert(QStringLiteral("type"), QStringLiteral("none"));
        break;
    case BackgroundType::Color:
        background.insert(QStringLiteral("type"), QStringLiteral("color"));
        background.insert(QStringLiteral("color"), m_background.color.name(QColor::HexArgb));
        break;
    case BackgroundType::Image:
        background.insert(QStringLiteral("type"), QStringLiteral("image"));
        background.insert(QStringLiteral("mode"), backgroundModeToString(m_background.mode));
        background.insert(QStringLiteral("image"), encodePixmap(m_background.image));
        break;
    }
    root.insert(QStringLiteral("background"), background);

    // 组的父子关系：{"子组 id": "父组 id"}，用于还原嵌套分组的选择行为。
    QJsonObject groupParents;
    for (auto it = m_groupParents.constBegin(); it != m_groupParents.constEnd(); ++it) {
        if (it.key() > 0 && it.value() > 0) {
            groupParents.insert(QString::number(it.key()), it.value());
        }
    }
    if (!groupParents.isEmpty()) {
        root.insert(QStringLiteral("groupParents"), groupParents);
    }

    QJsonArray layerArray;
    for (const CanvasLayer& entry : m_layers) {
        QJsonObject layerJson;
        layerJson.insert(QStringLiteral("id"), entry.id);
        layerJson.insert(QStringLiteral("name"), entry.name);
        layerJson.insert(QStringLiteral("visible"), entry.visible);
        layerJson.insert(QStringLiteral("locked"), entry.locked);
        layerArray.append(layerJson);
    }
    root.insert(QStringLiteral("layers"), layerArray);
    root.insert(QStringLiteral("activeLayer"), m_activeLayerId);

    QJsonArray itemsArray;
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        QJsonObject itemJson = item->toJson();
        // 旧画布只有图形自带 opacity，这里统一给所有图元写入透明度。
        itemJson.insert(QStringLiteral("opacity"), item->itemOpacity());
        itemsArray.append(itemJson);
    }
    root.insert(QStringLiteral("items"), itemsArray);

    return root;
}

bool CanvasScene::fromJson(const QJsonObject& root, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("StoryBoardCanvas")) {
        return fail(tr("This is not a valid StoryBoard canvas file."));
    }

    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    // 画布尺寸允许带小数（拖拽边缘时通常不是整数），这里不能用 toInt()。
    const qreal width = canvas.value(QStringLiteral("width")).toDouble(0.0);
    const qreal height = canvas.value(QStringLiteral("height")).toDouble(0.0);
    if (width <= 0.0 || height <= 0.0) {
        return fail(tr("The canvas size in the canvas file is invalid."));
    }

    resetDocument();
    applyCanvasRect(QRectF(canvas.value(QStringLiteral("x")).toDouble(0.0),
                           canvas.value(QStringLiteral("y")).toDouble(0.0), width, height));

    BackgroundState background;
    const QJsonObject backgroundJson = root.value(QStringLiteral("background")).toObject();
    const QString backgroundType = backgroundJson.value(QStringLiteral("type")).toString();
    if (backgroundType == QStringLiteral("color")) {
        const QColor color(backgroundJson.value(QStringLiteral("color")).toString());
        background.type = BackgroundType::Color;
        background.color = color.isValid() ? color : QColor(255, 255, 255);
    } else if (backgroundType == QStringLiteral("image")) {
        const QPixmap pixmap = decodePixmap(backgroundJson.value(QStringLiteral("image")).toString());
        if (!pixmap.isNull()) {
            background.type = BackgroundType::Image;
            background.image = pixmap;
            background.mode = backgroundModeFromString(backgroundJson.value(QStringLiteral("mode")).toString());
        }
    }
    applyBackground(background);

    const QJsonArray rawItems = root.value(QStringLiteral("items")).toArray();
    QList<QJsonObject> itemObjects;
    itemObjects.reserve(rawItems.size());
    for (const QJsonValue& value : rawItems) {
        itemObjects.append(value.toObject());
    }

    const QJsonArray layerArray = root.value(QStringLiteral("layers")).toArray();
    if (!root.value(QStringLiteral("layers")).isArray() || layerArray.isEmpty()) {
        return fail(tr("This is not a valid StoryBoard canvas file."));
    }

    LayerStackState state;
    bool backgroundVisible = true;
    state.layers.append({BackgroundLayerId, scene_constants::kBackgroundLayerName, true, true});
    QSet<int> used;
    used.insert(BackgroundLayerId);
    for (const QJsonValue& value : layerArray) {
        const QJsonObject layerJson = value.toObject();
        const int id = layerJson.value(QStringLiteral("id")).toInt();
        if (id == BackgroundLayerId) {
            backgroundVisible = layerJson.value(QStringLiteral("visible")).toBool(true);
            continue;
        }
        if (id <= 0 || used.contains(id)) {
            continue;
        }
        CanvasLayer entry;
        entry.id = id;
        entry.name = layerJson.value(QStringLiteral("name")).toString().trimmed();
        entry.visible = layerJson.value(QStringLiteral("visible")).toBool(true);
        entry.locked = layerJson.value(QStringLiteral("locked")).toBool(false);
        if (entry.name.isEmpty()) {
            entry.name = QStringLiteral("图层 %1").arg(id);
        }
        state.layers.append(entry);
        used.insert(id);
    }
    state.layers[0].visible = backgroundVisible;
    state.activeLayerId = root.value(QStringLiteral("activeLayer")).toInt(scene_constants::kFirstUserLayerId);

    // 图元引用了未出现在图层数组中的图层时，补出该图层，避免图元丢失。
    for (const QJsonObject& itemJson : std::as_const(itemObjects)) {
        const int layerId = itemJson.value(QStringLiteral("layer")).toInt();
        if (layerId <= 0) {
            continue;
        }
        const bool exists = std::any_of(state.layers.cbegin(), state.layers.cend(),
                                        [layerId](const CanvasLayer& entry) { return entry.id == layerId; });
        if (!exists) {
            state.layers.append({layerId, QStringLiteral("图层 %1").arg(layerId)});
        }
    }

    applyLayerStateInternal(sanitizedLayerState(state), false);

    for (int i = 0; i < itemObjects.size(); ++i) {
        const QJsonObject itemJson = itemObjects.at(i);
        const QString kind = itemJson.value(QStringLiteral("type")).toString();
        if (kind == QStringLiteral("stroke")) {
            // 旧笔画图元不兼容新曲线：跳过，不因单个旧图元阻断整份画布。
            continue;
        }
        CanvasItem* item = createItem(kind);
        if (!item) {
            resetDocument();
            return fail(tr("The canvas contains an unknown item type: %1").arg(kind));
        }
        item->fromJson(itemJson);
        item->setItemOpacity(itemJson.value(QStringLiteral("opacity")).toDouble(1.0));
        if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId) {
            item->setLayerId(m_activeLayerId);
        }
        addItem(item);
    }

    m_nextGroupId = 1;
    for (CanvasItem* item : canvasItems()) {
        m_nextGroupId = qMax(m_nextGroupId, item->groupId() + 1);
    }
    // 还原嵌套分组的父子关系（旧文件没有这个字段，按平铺分组处理）。
    QHash<int, int> parents;
    const QJsonObject parentsJson = root.value(QStringLiteral("groupParents")).toObject();
    for (auto it = parentsJson.constBegin(); it != parentsJson.constEnd(); ++it) {
        const int child = it.key().toInt();
        const int parent = it.value().toInt();
        if (child > 0 && parent > 0 && child != parent) {
            parents.insert(child, parent);
        }
    }
    applyGroupParents(parents);

    // 旧画布没有 itemName；补自动名称，保证重新保存后每个图元都有“类型 N”。
    assignAutoItemNames();

    // Assign heights for new-format items that omitted the property, then
    // normalize any duplicate heights in a layer while keeping their order.
    QHash<int, int> nextHeights;
    const QList<CanvasItem*> items = canvasItems();
    for (CanvasItem* item : items) {
        if (item->height() >= 0) {
            continue;
        }
        auto it = nextHeights.constFind(item->layerId());
        int next = 0;
        if (it == nextHeights.constEnd()) {
            next = nextHeight(item->layerId());
            nextHeights.insert(item->layerId(), next);
        } else {
            next = it.value();
        }
        item->setHeight(qMin(scene_constants::kMaxItemHeight, next));
        nextHeights[item->layerId()] = qMin(scene_constants::kMaxItemHeight, next + 1);
    }

    // Enforce the canvas invariant: every layer has one item per height.
    // The stable sort inside normalizeLayerHeights preserves the stored order
    // when duplicate values are repaired.
    normalizeAllHeights();

    m_nextLayerId = 1;
    for (const CanvasLayer& entry : std::as_const(m_layers)) {
        m_nextLayerId = qMax(m_nextLayerId, entry.id + 1);
    }

    ensureLayerState();
    applyAllItemStacking();
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    emit backgroundChanged();
    emit canvasSizeChanged(m_canvasSize);
    return true;
}
