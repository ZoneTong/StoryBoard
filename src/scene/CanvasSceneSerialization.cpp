#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CurveItem.h"
#include "ImageStore.h"
#include "ShapeItem.h"
#include "ShapePathBuilder.h"
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

QJsonObject CanvasScene::ToJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    root.insert(QStringLiteral("version"), 2);
    // 版本号随 CMake project VERSION 注入（STORYBOARD_VERSION），不再手写漂移。
    root.insert(QStringLiteral("generator"),
                QStringLiteral("StoryBoard %1").arg(QString::fromLatin1(STORYBOARD_VERSION)));

    QJsonObject canvas;
    canvas.insert(QStringLiteral("x"), m_canvasOrigin.x());
    canvas.insert(QStringLiteral("y"), m_canvasOrigin.y());
    canvas.insert(QStringLiteral("width"), m_canvasSize.width());
    canvas.insert(QStringLiteral("height"), m_canvasSize.height());
    root.insert(QStringLiteral("canvas"), canvas);

    QJsonObject background;
    switch (m_background.type) {
    case BackgroundType::NONE:
        background.insert(QStringLiteral("type"), QStringLiteral("none"));
        break;
    case BackgroundType::COLOR:
        background.insert(QStringLiteral("type"), QStringLiteral("color"));
        background.insert(QStringLiteral("color"), m_background.color.name(QColor::HexArgb));
        break;
    case BackgroundType::IMAGE:
        background.insert(QStringLiteral("type"), QStringLiteral("image"));
        background.insert(QStringLiteral("mode"), BackgroundModeToString(m_background.mode));
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
    // 需求 11：同一张图片在文件里只存一份。保存前按内容哈希收集本画布用到的图片，
    // 表里只留仍被引用的项，零引用的图片在保存后自然消失。
    QJsonObject imagesTable;
    QSet<QString> usedImageIds;
    const QList<CanvasItem*> items = CanvasItems();
    for (CanvasItem* item : items) {
        QJsonObject itemJson = item->ToJson();
        // 旧画布只有图形自带 opacity，这里统一给所有图元写入透明度。
        itemJson.insert(QStringLiteral("opacity"), item->ItemOpacity());
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->HasFillImage()) {
            // 以图元当前像素重新签发 id：复制 / 粘贴 / 换图后内容与 id 必然一致。
            const QString id = ImageStore::Store(&imagesTable, shape->FillImage());
            if (!id.isEmpty()) {
                usedImageIds.insert(id);
                itemJson.insert(QStringLiteral("fillImageId"), id);
                itemJson.insert(QStringLiteral("fillImageMode"), ImageFill::ModeToString(shape->FillImageMode()));
                itemJson.remove(QStringLiteral("fillImage"));
            }
        }
        itemsArray.append(itemJson);
    }
    if (!usedImageIds.isEmpty()) {
        root.insert(QStringLiteral("images"), ImageStore::Compact(imagesTable, usedImageIds));
    }
    root.insert(QStringLiteral("items"), itemsArray);

    return root;
}

bool CanvasScene::FromJson(const QJsonObject& root, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    // 打开失败绝不许动当前文档：下面所有校验与预解析都通过之前不调用
    // ResetDocument()，任何一步失败都直接返回。否则「打开失败清空用户正在
    // 编辑的内容、m_filePath 仍指旧文件、一次保存把空画布写回原文件」。
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

    const QJsonArray layerArray = root.value(QStringLiteral("layers")).toArray();
    if (!root.value(QStringLiteral("layers")).isArray() || layerArray.isEmpty()) {
        return fail(tr("This is not a valid StoryBoard canvas file."));
    }

    const QJsonArray rawItems = root.value(QStringLiteral("items")).toArray();
    QList<QJsonObject> itemObjects;
    itemObjects.reserve(rawItems.size());
    for (const QJsonValue& value : rawItems) {
        itemObjects.append(value.toObject());
    }

    // 预创建全部图元，把未知类型拦在动文档之前；失败时释放已创建的副本。
    // 需求 11：画布级图片表先解出来，图元只带 id，图片本体从这里取。
    const QJsonObject imagesTable = root.value(QStringLiteral("images")).toObject();
    QList<CanvasItem*> parsedItems;
    for (const QJsonObject& itemJson : std::as_const(itemObjects)) {
        const QString kind = itemJson.value(QStringLiteral("kind")).toString();
        if (kind == QStringLiteral("stroke")) {
            // 旧笔画图元不兼容新曲线：跳过，不因单个旧图元阻断整份画布。
            continue;
        }
        // 不兼容任何旧画布格式：曲线族（含花括号、正弦）统一走工厂。
        CanvasItem* item = CreateItemForJson(itemJson);
        if (!item) {
            qDeleteAll(parsedItems);
            return fail(tr("The canvas contains an unknown item type: %1").arg(kind));
        }
        item->FromJson(itemJson);
        item->SetItemOpacity(itemJson.value(QStringLiteral("opacity")).toDouble(1.0));
        // 需求 11：引用式背景图片；表里缺项（例如手工改过文件）时按无图处理，不阻断打开。
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && !shape->FillImageId().isEmpty()) {
            const QPixmap pixmap = ImageStore::PixmapFromEntry(imagesTable.value(shape->FillImageId()).toObject());
            if (!pixmap.isNull()) {
                shape->SetFillImage(pixmap, shape->FillImageMode());
                shape->SetFillImageId(itemJson.value(QStringLiteral("fillImageId")).toString());
            }
        }
        parsedItems.append(item);
    }

    // ---- 校验全部通过，从这里开始才清空重建 ----
    m_restoringDocument = true;
    ResetDocument();

    // 画布尺寸按文件原值恢复，不做 200–12000 钳位：钳位只应发生在编辑操作里，
    // 否则超出范围的旧文件一打开、再保存就被静默改写。
    m_canvasOrigin =
        QPointF(canvas.value(QStringLiteral("x")).toDouble(0.0), canvas.value(QStringLiteral("y")).toDouble(0.0));
    m_canvasSize = QSizeF(width, height);

    CanvasBackground background;
    const QJsonObject backgroundJson = root.value(QStringLiteral("background")).toObject();
    const QString backgroundType = backgroundJson.value(QStringLiteral("type")).toString();
    if (backgroundType == QStringLiteral("color")) {
        const QColor color(backgroundJson.value(QStringLiteral("color")).toString());
        background.type = BackgroundType::COLOR;
        background.color = color.isValid() ? color : QColor(255, 255, 255);
    } else if (backgroundType == QStringLiteral("image")) {
        const QPixmap pixmap = decodePixmap(backgroundJson.value(QStringLiteral("image")).toString());
        if (!pixmap.isNull()) {
            background.type = BackgroundType::IMAGE;
            background.image = pixmap;
            background.mode = BackgroundModeFromString(backgroundJson.value(QStringLiteral("mode")).toString());
        }
    }
    SetCanvasBackground(background);

    LayerStackState state;
    bool backgroundVisible = true;
    state.layers.append({BackgroundLayerId, scene_constants::BACKGROUND_LAYER_NAME, true, true});
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
    state.activeLayerId = root.value(QStringLiteral("activeLayer")).toInt(scene_constants::FIRST_USER_LAYER_ID);

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

    ApplyLayerStateInternal(SanitizedLayerState(state), false);

    for (CanvasItem* item : std::as_const(parsedItems)) {
        if (!HasLayer(item->LayerId()) || item->LayerId() == BackgroundLayerId) {
            item->SetLayerId(m_activeLayerId);
        }
        addItem(item);
    }

    m_nextGroupId = 1;
    for (CanvasItem* item : CanvasItems()) {
        m_nextGroupId = qMax(m_nextGroupId, item->GroupId() + 1);
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
    ApplyGroupParents(parents);

    // 旧画布没有 itemName；补自动名称，保证重新保存后每个图元都有“类型 N”。
    AssignAutoItemNames();

    // Assign heights for new-format items that omitted the property, then
    // normalize any duplicate heights in a layer while keeping their order.
    QHash<int, int> nextHeights;
    QHash<int, QSet<int>> usedHeights;
    const QList<CanvasItem*> items = CanvasItems();
    for (const CanvasItem* item : items) {
        usedHeights[item->LayerId()].insert(item->ZOrder());
    }
    for (CanvasItem* item : items) {
        if (item->ZOrder() >= 0) {
            continue;
        }
        auto it = nextHeights.constFind(item->LayerId());
        int next = 0;
        if (it == nextHeights.constEnd()) {
            next = NextHeight(item->LayerId());
            nextHeights.insert(item->LayerId(), next);
        } else {
            next = it.value();
        }
        // 层满时 nextHeight 返回 -1：退化为取同层已用最大高度 +1；只有
        // 0–999999 真排满才会继续失败，那时交给下面的 normalize 修复，
        // 不再把非法值写进图元。
        if (next < 0) {
            next = 0;
            for (const int height : std::as_const(usedHeights[item->LayerId()])) {
                next = qMax(next, height + 1);
            }
        }
        item->SetZOrder(qMin(scene_constants::MAX_ITEM_HEIGHT, next));
        nextHeights[item->LayerId()] = qMin(scene_constants::MAX_ITEM_HEIGHT, next + 1);
        usedHeights[item->LayerId()].insert(item->ZOrder());
    }

    // Enforce the canvas invariant: every layer has one item per height.
    // The stable sort inside normalizeLayerHeights preserves the stored order
    // when duplicate values are repaired.
    NormalizeAllHeights();

    m_nextLayerId = 1;
    for (const CanvasLayer& entry : std::as_const(m_layers)) {
        m_nextLayerId = qMax(m_nextLayerId, entry.id + 1);
    }

    m_restoringDocument = false;
    EnsureLayerState();
    ApplyAllItemStacking();
    UpdateItemFlags();
    UpdateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    emit backgroundChanged();
    emit canvasSizeChanged(m_canvasSize);
    return true;
}
