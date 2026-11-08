#include "I18n.h"

#include "BoardScene.h"

#include "BoardItem.h"
#include "Commands.h"
#include "ImageItem.h"
#include "ShapeItem.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QBuffer>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QRegion>
#include <QSet>
#include <QStyleOptionGraphicsItem>
#include <QUndoStack>
#include <QVector>
#include <QtMath>

#include <algorithm>
#include <utility>

namespace {

constexpr qreal kMinBoardSide = 200.0;
constexpr qreal kMaxBoardSide = 12000.0;
constexpr int kMaxItemHeight = 999999;
constexpr qreal kLayerZStride = 1000000.0;
constexpr int kFirstUserLayerId = 1;
const QString kBackgroundLayerName = QStringLiteral("背景");

bool nearlyEqual(qreal a, qreal b)
{
    return qAbs(a - b) < 0.01;
}

bool sameGeometry(const ItemGeometry& a, const ItemGeometry& b)
{
    return nearlyEqual(a.pos.x(), b.pos.x())
        && nearlyEqual(a.pos.y(), b.pos.y())
        && nearlyEqual(a.size.width(), b.size.width())
        && nearlyEqual(a.size.height(), b.size.height());
}

QString encodePixmap(const QPixmap& pixmap)
{
    if (pixmap.isNull())
        return QString();
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
    if (!bytes.isEmpty())
        pixmap.loadFromData(bytes, "PNG");
    return pixmap;
}

QRegion filledRegion(const QPainterPath& path)
{
    QRegion region;
    if (path.isEmpty())
        return region;
    // 用路径填充多边形近似成区域，便于快速算出被遮盖的面积占比。
    for (const QPolygonF& polygon : path.toFillPolygons()) {
        const QPolygon points = polygon.toPolygon();
        if (points.size() >= 3)
            region = region.united(QRegion(points));
    }
    return region;
}

qreal filledRegionArea(const QRegion& region)
{
    qreal area = 0.0;
    for (const QRect& rect : region)
        area += qreal(rect.width()) * qreal(rect.height());
    return area;
}

} // namespace

BoardScene::BoardScene(QObject* parent)
    : QGraphicsScene(parent)
{
    m_layers = {
        { BackgroundLayerId, kBackgroundLayerName, true, true },
        { kFirstUserLayerId, QStringLiteral("图层 1"), true, false }
    };
    m_activeLayerId = kFirstUserLayerId;
    m_nextLayerId = kFirstUserLayerId + 1;

    setBackgroundBrush(Qt::NoBrush);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    setStickyFocus(true);
    updateSceneExtent();
}

BoardScene::~BoardScene()
{
    clearOrphans();
}

void BoardScene::setUndoStack(QUndoStack* stack)
{
    m_undoStack = stack;
}

void BoardScene::setMode(ToolMode mode)
{
    if (m_mode == mode)
        return;

    if (m_mode == ToolMode::Edit)
        clearSelection();

    m_mode = mode;
    updateItemFlags();
    update();
    emit modeChanged(mode);
}

void BoardScene::updateItemFlags()
{
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items)
        applyItemInteraction(item);
}

void BoardScene::applyItemInteraction(BoardItem* item)
{
    if (!item)
        return;

    const bool layerVisible = isLayerVisible(item->layerId());
    const bool editable = m_mode == ToolMode::Edit
        && layerVisible
        && !isLayerLocked(item->layerId());

    item->setVisible(layerVisible);
    item->setFlag(QGraphicsItem::ItemIsSelectable, editable);
    item->setAcceptHoverEvents(editable);
    if (!editable)
        item->setSelected(false);
}

QRectF BoardScene::boardRect() const
{
    return QRectF(m_boardOrigin, m_boardSize);
}

void BoardScene::applyBoardRect(const QRectF& rect)
{
    const QRectF normalized = rect.normalized();
    const QSizeF bounded(qBound(kMinBoardSide, normalized.width(), kMaxBoardSide),
                         qBound(kMinBoardSide, normalized.height(), kMaxBoardSide));
    const QPointF origin(normalized.left(), normalized.top());
    if (qFuzzyCompare(bounded.width(), m_boardSize.width())
        && qFuzzyCompare(bounded.height(), m_boardSize.height())
        && origin == m_boardOrigin) {
        return;
    }
    m_boardOrigin = origin;
    m_boardSize = bounded;
    updateSceneExtent();
    update();
    emit boardSizeChanged(m_boardSize);
}

void BoardScene::applyBoardSize(const QSizeF& size)
{
    applyBoardRect(QRectF(m_boardOrigin, size));
}

void BoardScene::setBoardSizeWithUndo(const QSizeF& size)
{
    setBoardRectWithUndo(QRectF(m_boardOrigin, size));
}

void BoardScene::setBoardRectWithUndo(const QRectF& rect)
{
    const QRectF before = boardRect();
    applyBoardRect(rect);
    commitBoardRect(before);
}

void BoardScene::commitBoardRect(const QRectF& before)
{
    const QRectF after = boardRect();
    if (before == after)
        return;
    if (m_undoStack)
        m_undoStack->push(new BoardSizeCommand(this, before, after,
                                              uiText(QStringLiteral("Resize canvas"))));
}

void BoardScene::applyBackground(const BackgroundState& background)
{
    m_background = background;
    update();
    emit backgroundChanged();
}

void BoardScene::replaceBackground(const BackgroundState& background, const QString& text)
{
    if (background == m_background)
        return;

    if (m_undoStack) {
        m_undoStack->push(new BackgroundCommand(this, m_background, background,
                                               text.isEmpty() ? uiText(QStringLiteral("Change background")) : text));
    } else {
        applyBackground(background);
    }
}

void BoardScene::setShowGrid(bool enabled)
{
    if (m_showGrid == enabled)
        return;
    m_showGrid = enabled;
    update();
}

QRectF BoardScene::contentRect() const
{
    QRectF rect;
    bool hasItem = false;
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items) {
        if (!item->isVisible())
            continue;
        const QRectF itemRect = item->sceneContentRect();
        if (itemRect.isEmpty())
            continue;
        if (!hasItem) {
            rect = itemRect;
            hasItem = true;
        } else {
            rect = rect.united(itemRect);
        }
    }
    return hasItem ? rect : QRectF();
}

void BoardScene::updateSceneExtent()
{
    QRectF extent = boardRect();
    const QRectF content = contentRect();
    if (!content.isEmpty())
        extent = extent.united(content);
    extent = extent.adjusted(-400.0, -400.0, 400.0, 400.0);
    if (sceneRect() != extent)
        setSceneRect(extent);
}

QList<BoardItem*> BoardScene::boardItems() const
{
    QList<BoardItem*> result;
    const QList<QGraphicsItem*> allItems = items(Qt::AscendingOrder);
    result.reserve(allItems.size());
    for (QGraphicsItem* graphicsItem : allItems) {
        if (auto* item = qgraphicsitem_cast<BoardItem*>(graphicsItem))
            result.append(item);
    }
    return result;
}

int BoardScene::boardItemCount() const
{
    return boardItems().size();
}

QList<BoardItem*> BoardScene::obscuredItems(const QRectF& area) const
{
    QList<BoardItem*> result;

    // 查看模式下要求完全遮盖（什么提示都不画）；导出同样不画提示轮廓。
    if (m_mode == ToolMode::View || m_exporting)
        return result;

    QList<BoardItem*> stack = boardItems();
    stack.erase(std::remove_if(stack.begin(), stack.end(), [&area](BoardItem* item) {
        if (!item || !item->isVisible())
            return true;
        return !area.isEmpty() && !item->sceneBoundingRect().intersects(area);
    }), stack.end());
    std::stable_sort(stack.begin(), stack.end(), [](const BoardItem* a, const BoardItem* b) {
        return a->zValue() < b->zValue();
    });

    const int count = stack.size();
    QList<QRectF> bounds;
    QList<QRegion> regions;
    QVector<bool> regionReady;
    bounds.reserve(count);
    regions.reserve(count);
    regionReady.reserve(count);
    for (int i = 0; i < count; ++i) {
        bounds.append(stack.at(i)->sceneBoundingRect());
        regions.append(QRegion());
        regionReady.append(false);
    }

    // paintedShape() 会为笔画、图形做描边展开，只在真正需要比较时才计算。
    auto regionOf = [&stack, &regions, &regionReady](int index) -> const QRegion& {
        if (!regionReady.at(index)) {
            BoardItem* item = stack.at(index);
            regions[index] = filledRegion(item->sceneTransform().map(item->paintedShape()));
            regionReady[index] = true;
        }
        return regions.at(index);
    };

    for (int i = 0; i < count; ++i) {
        BoardItem* lower = stack.at(i);
        const QRegion own = regionOf(i);
        const qreal ownArea = filledRegionArea(own);
        if (ownArea <= 0.0)
            continue;

        QRegion covered;
        for (int j = i + 1; j < count; ++j) {
            BoardItem* upper = stack.at(j);
            // layer 更大，或同层 height 更大，就是更高优先级。
            if (upper->zValue() <= lower->zValue())
                continue;
            if (!bounds.at(j).intersects(bounds.at(i)))
                continue;
            covered = covered.united(own.intersected(regionOf(j)));
        }

        // 只要有任何面积被更高优先级图元盖住，就算被遮挡：被盖住的内容完全看不见，
        // 但会给它补一圈轮廓。
        if (filledRegionArea(covered) > 0.0)
            result.append(lower);
    }
    return result;
}

QList<BoardItem*> BoardScene::selectedBoardItems() const
{
    QList<BoardItem*> result;
    const QList<QGraphicsItem*> graphicsItems = selectedItems();
    for (QGraphicsItem* graphicsItem : graphicsItems) {
        if (auto* item = qgraphicsitem_cast<BoardItem*>(graphicsItem))
            result.append(item);
    }
    return result;
}

QList<BoardLayer> BoardScene::userLayers() const
{
    if (m_layers.size() <= 1)
        return {};
    return m_layers.mid(1);
}

bool BoardScene::hasLayer(int layerId) const
{
    return layerIndex(layerId) >= 0;
}

BoardLayer BoardScene::layer(int layerId) const
{
    for (const BoardLayer& entry : m_layers) {
        if (entry.id == layerId)
            return entry;
    }
    return {};
}

int BoardScene::layerIndex(int layerId) const
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers.at(i).id == layerId)
            return i;
    }
    return -1;
}

int BoardScene::layerItemCount(int layerId) const
{
    int count = 0;
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items) {
        if (item->layerId() == layerId)
            ++count;
    }
    return count;
}

QList<BoardItem*> BoardScene::layerItems(int layerId) const
{
    QList<BoardItem*> result;
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items) {
        if (item->layerId() == layerId)
            result.append(item);
    }
    return result;
}

bool BoardScene::isLayerVisible(int layerId) const
{
    return hasLayer(layerId) && layer(layerId).visible;
}

bool BoardScene::setLayerVisible(int layerId, bool visible)
{
    const int index = layerIndex(layerId);
    if (index < 0)
        return false;

    if (m_layers.at(index).visible == visible)
        return true;

    bool activeChanged = false;
    if (!visible && layerId == m_activeLayerId && layerId != BackgroundLayerId) {
        // The active layer is always visible. Hiding it moves the selection to
        // the nearest editable layer instead of leaving an invisible active
        // layer that cannot be edited.
        int fallbackIndex = -1;
        for (int distance = 1; distance < m_layers.size(); ++distance) {
            const int below = index - distance;
            if (below > 0) {
                fallbackIndex = below;
                break;
            }
            const int above = index + distance;
            if (above < m_layers.size()) {
                fallbackIndex = above;
                break;
            }
        }
        if (fallbackIndex < 0)
            return false;

        m_layers[fallbackIndex].visible = true;
        m_activeLayerId = m_layers.at(fallbackIndex).id;
        activeChanged = true;
    }

    m_layers[index].visible = visible;
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    if (activeChanged)
        emit activeLayerChanged(m_activeLayerId);
    return true;
}

bool BoardScene::isLayerLocked(int layerId) const
{
    return hasLayer(layerId) && layer(layerId).locked;
}

bool BoardScene::setAllLayersVisible(bool visible)
{
    bool changed = false;
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers.at(i).visible != visible) {
            m_layers[i].visible = visible;
            changed = true;
        }
    }
    if (!changed)
        return true;

    // 表头勾选框是批量浏览操作：允许一次隐藏全部图层（包括当前图层），
    // 不套用单行勾选框“至少保留一个可见用户图层”的限制。
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    return true;
}

bool BoardScene::setLayerLocked(int layerId, bool locked)
{
    // 背景层始终锁定：它只承载背景，没有可编辑图元，也不允许解锁。
    if (layerId == BackgroundLayerId)
        return false;

    const int index = layerIndex(layerId);
    if (index < 0)
        return false;
    if (m_layers.at(index).locked == locked)
        return true;

    m_layers[index].locked = locked;
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit contentChanged();
    return true;
}

bool BoardScene::isActiveLayerWritable() const
{
    return m_activeLayerId != BackgroundLayerId
        && hasLayer(m_activeLayerId)
        && isLayerVisible(m_activeLayerId)
        && !isLayerLocked(m_activeLayerId);
}

bool BoardScene::isItemEditable(const BoardItem* item) const
{
    // 需求变更后的口径：编辑模式下，只要图元所在图层可见且未锁定就能编辑，
    // 与“当前图层”无关。
    return item
        && item->scene() == this
        && m_mode == ToolMode::Edit
        && isLayerVisible(item->layerId())
        && !isLayerLocked(item->layerId());
}

void BoardScene::setActiveLayer(int layerId)
{
    if (layerId == BackgroundLayerId || !hasLayer(layerId))
        return;

    bool visibilityChanged = false;
    if (!isLayerVisible(layerId)) {
        for (BoardLayer& entry : m_layers) {
            if (entry.id == layerId) {
                entry.visible = true;
                break;
            }
        }
        visibilityChanged = true;
    }

    if (layerId == m_activeLayerId) {
        if (visibilityChanged) {
            updateItemFlags();
            updateSceneExtent();
            update();
            emit layersChanged();
            emit contentChanged();
        }
        return;
    }

    m_activeLayerId = layerId;
    updateItemFlags();
    updateSceneExtent();
    update();
    if (visibilityChanged)
        emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    if (visibilityChanged)
        emit contentChanged();
}

int BoardScene::createLayer(const QString& name)
{
    const int id = m_nextLayerId++;
    BoardLayer entry;
    entry.id = id;
    entry.name = name.trimmed().isEmpty() ? QStringLiteral("图层 %1").arg(id) : name.trimmed();
    m_layers.append(entry);
    m_activeLayerId = id;
    applyAllItemStacking();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    return id;
}

bool BoardScene::addLayerWithUndo(const QString& name)
{
    const LayerStackState before = layerState();
    const int id = m_nextLayerId++;
    BoardLayer entry;
    entry.id = id;
    entry.name = name.trimmed().isEmpty() ? QStringLiteral("图层 %1").arg(id) : name.trimmed();

    LayerStackState after = before;
    after.layers.append(entry);
    after.activeLayerId = id;
    const QString text = uiText(QStringLiteral("New layer"));

    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

bool BoardScene::removeLayerWithUndo(int layerId, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    const int index = layerIndex(layerId);
    if (index <= 0)
        return fail(uiText(QStringLiteral("The background layer cannot be deleted.")));
    if (userLayerCount() <= 1)
        return fail(uiText(QStringLiteral("At least one layer must remain.")));
    if (layerItemCount(layerId) > 0)
        return fail(uiText(QStringLiteral("This layer still contains items. Delete or move them first.")));

    const LayerStackState before = layerState();
    LayerStackState after = before;
    after.layers.removeAt(index);
    if (after.activeLayerId == layerId) {
        const int fallbackIndex = qBound(1, index - 1, static_cast<int>(after.layers.size()) - 1);
        after.activeLayerId = after.layers.at(fallbackIndex).id;
    }

    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, uiText(QStringLiteral("Delete layer"))));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

bool BoardScene::moveLayerWithUndo(int layerId, int delta)
{
    const int index = layerIndex(layerId);
    if (index <= 0 || delta == 0)
        return false;

    const int target = index + delta;
    if (target <= 0 || target >= m_layers.size())
        return false;

    const LayerStackState before = layerState();
    LayerStackState after = before;
    after.layers.swapItemsAt(index, target);

    const QString text = delta > 0 ? uiText(QStringLiteral("Move layer up")) : uiText(QStringLiteral("Move layer down"));
    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

bool BoardScene::renameLayerWithUndo(int layerId, const QString& name)
{
    const int index = layerIndex(layerId);
    if (index <= 0)
        return false;

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_layers.at(index).name == trimmed)
        return false;

    const LayerStackState before = layerState();
    LayerStackState after = before;
    after.layers[index].name = trimmed;

    const QString text = uiText(QStringLiteral("Rename layer"));
    if (m_undoStack) {
        m_undoStack->push(new LayerStateCommand(this, before, after, text));
    } else {
        applyLayerStateInternal(after);
    }
    return true;
}

LayerStackState BoardScene::layerState() const
{
    LayerStackState state;
    state.layers = m_layers;
    state.activeLayerId = m_activeLayerId;
    return state;
}

void BoardScene::applyLayerState(const LayerStackState& state)
{
    applyLayerStateInternal(sanitizedLayerState(state));
}

LayerStackState BoardScene::sanitizedLayerState(const LayerStackState& state) const
{
    LayerStackState result;
    bool backgroundVisible = true;
    for (const BoardLayer& source : state.layers) {
        if (source.id == BackgroundLayerId) {
            backgroundVisible = source.visible;
            break;
        }
    }
    result.layers.append({ BackgroundLayerId, kBackgroundLayerName, backgroundVisible, true });

    QSet<int> used;
    used.insert(BackgroundLayerId);
    for (const BoardLayer& source : state.layers) {
        if (source.id <= 0 || used.contains(source.id))
            continue;
        BoardLayer entry = source;
        if (entry.name.trimmed().isEmpty())
            entry.name = QStringLiteral("图层 %1").arg(entry.id);
        result.layers.append(entry);
        used.insert(entry.id);
    }

    if (result.layers.size() == 1) {
        int fallbackId = kFirstUserLayerId;
        while (used.contains(fallbackId))
            ++fallbackId;
        result.layers.append({ fallbackId, QStringLiteral("图层 %1").arg(fallbackId), true, false });
    }

    result.activeLayerId = state.activeLayerId;
    if (result.activeLayerId == BackgroundLayerId || !used.contains(result.activeLayerId)) {
        result.activeLayerId = result.layers.at(1).id;
    }
    for (BoardLayer& entry : result.layers) {
        if (entry.id == result.activeLayerId) {
            entry.visible = true;
            break;
        }
    }
    return result;
}

void BoardScene::applyLayerStateInternal(const LayerStackState& state, bool preserveLock)
{
    const QList<BoardLayer> previousLayers = m_layers;
    m_layers = state.layers;
    m_activeLayerId = state.activeLayerId;

    // 锁定状态刻意不进入撤销栈：撤销/重做图层操作（新建、删除、排序、显隐）
    // 不应该顺带改变某一层的锁定。这里用同 id 的旧快照把 locked 覆盖回来；
    // 但从画布载入时要保留文件里的锁定状态，因此传入 preserveLock=false。
    if (preserveLock) {
        for (BoardLayer& entry : m_layers) {
            for (const BoardLayer& previous : previousLayers) {
                if (previous.id == entry.id) {
                    entry.locked = previous.locked;
                    break;
                }
            }
        }
    }

    for (BoardLayer& entry : m_layers) {
        if (entry.id == m_activeLayerId && !entry.visible)
            entry.visible = true;
    }

    for (const BoardLayer& entry : m_layers)
        m_nextLayerId = qMax(m_nextLayerId, entry.id + 1);

    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items) {
        if (!hasLayer(item->layerId()))
            item->setLayerId(m_activeLayerId);
    }
    normalizeAllHeights();
    applyAllItemStacking();
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
}

void BoardScene::ensureLayerState()
{
    const LayerStackState fixed = sanitizedLayerState(layerState());
    if (fixed.layers != m_layers || fixed.activeLayerId != m_activeLayerId)
        applyLayerStateInternal(fixed, false);
}

qreal BoardScene::layerBaseZ(int layerId) const
{
    const int index = layerIndex(layerId);
    return index > 0 ? index * kLayerZStride : 0.0;
}

void BoardScene::applyItemStacking(BoardItem* item)
{
    if (!item)
        return;
    const int maxHeight = kMaxItemHeight;
    const int height = qBound(0, item->height(), maxHeight);
    if (height != item->height())
        item->setHeight(height);
    item->setZValue(layerBaseZ(item->layerId()) + height);
}

void BoardScene::applyAllItemStacking()
{
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items)
        applyItemStacking(item);
}

int BoardScene::nextHeight(int layerId) const
{
    QSet<int> used;
    int maximum = 0;
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items) {
        if (item->layerId() != layerId || item->height() < 0)
            continue;
        used.insert(item->height());
        maximum = qMax(maximum, item->height());
    }

    const int aboveTop = maximum + 1;
    if (aboveTop <= kMaxItemHeight && !used.contains(aboveTop))
        return aboveTop;

    // The layer is effectively full (or reached the cap). Fill the first gap
    // instead of handing out a duplicate height.
    for (int height = 0; height <= kMaxItemHeight; ++height) {
        if (!used.contains(height))
            return height;
    }
    return -1;
}

bool BoardScene::layerHeightsUnique(int layerId) const
{
    QSet<int> heights;
    const QList<BoardItem*> items = layerItems(layerId);
    for (BoardItem* item : items) {
        if (item->height() < 0 || item->height() > kMaxItemHeight
            || heights.contains(item->height())) {
            return false;
        }
        heights.insert(item->height());
    }
    return true;
}

bool BoardScene::heightUsedByOther(int layerId, int height, const BoardItem* except) const
{
    const QList<BoardItem*> items = layerItems(layerId);
    for (const BoardItem* item : items) {
        if (item != except && item->height() == height)
            return true;
    }
    return false;
}

bool BoardScene::normalizeLayerHeights(int layerId)
{
    QList<BoardItem*> items = layerItems(layerId);
    if (items.isEmpty())
        return false;

    QSet<int> seen;
    bool needsNormalization = false;
    for (const BoardItem* item : std::as_const(items)) {
        if (item->height() < 0 || item->height() > kMaxItemHeight
            || seen.contains(item->height())) {
            needsNormalization = true;
            break;
        }
        seen.insert(item->height());
    }
    if (!needsNormalization)
        return false;

    std::stable_sort(items.begin(), items.end(),
                     [](const BoardItem* a, const BoardItem* b) {
                         if (a->height() != b->height())
                             return a->height() < b->height();
                         return a->zValue() < b->zValue();
                     });
    for (int i = 0; i < items.size(); ++i) {
        BoardItem* item = items.at(i);
        item->setHeight(i + 1);
        applyItemStacking(item);
        item->update();
    }
    return true;
}

void BoardScene::normalizeAllHeights()
{
    for (const BoardLayer& entry : std::as_const(m_layers)) {
        if (entry.id != BackgroundLayerId)
            normalizeLayerHeights(entry.id);
    }
}

ItemStackList BoardScene::itemStackSnapshot(const QList<BoardItem*>& items) const
{
    ItemStackList snapshot;
    for (BoardItem* item : items) {
        if (item)
            snapshot.append({ item, item->layerId(), item->height() });
    }
    return snapshot;
}

bool BoardScene::adjustItemHeight(BoardItem* item, int delta, const QString& text)
{
    if (!item || delta == 0 || !hasLayer(item->layerId()))
        return false;

    const int current = item->height();
    const int target = qBound(0, current + delta, kMaxItemHeight);
    if (target == current)
        return false;

    BoardItem* colliding = nullptr;
    const QList<BoardItem*> peers = layerItems(item->layerId());
    for (BoardItem* peer : peers) {
        if (peer != item && peer->height() == target) {
            colliding = peer;
            break;
        }
    }

    const ItemStackList before = itemStackSnapshot(
        colliding ? QList<BoardItem*> { item, colliding } : QList<BoardItem*> { item });
    ItemStackList after;
    after.append({ item, item->layerId(), target });
    if (colliding)
        after.append({ colliding, colliding->layerId(), current });

    const QString commandText = text.isEmpty()
        ? (delta > 0 ? uiText(QStringLiteral("Move item up")) : uiText(QStringLiteral("Move item down")))
        : text;
    if (m_undoStack)
        m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
    else
        applyItemStack(after);
    return true;
}

int BoardScene::adjustSelectedHeights(int delta)
{
    if (delta == 0)
        return 0;

    QList<BoardItem*> targets;
    const QList<BoardItem*> selected = selectedBoardItems();
    for (BoardItem* item : selected) {
        if (item && isItemEditable(item))
            targets.append(item);
    }
    if (targets.isEmpty())
        return 0;

    const bool grouped = m_undoStack && targets.size() > 1;
    if (grouped)
        m_undoStack->beginMacro(delta > 0 ? uiText(QStringLiteral("Move item up"))
                                         : uiText(QStringLiteral("Move item down")));

    int changed = 0;
    for (BoardItem* item : targets) {
        if (adjustItemHeight(item, delta))
            ++changed;
    }

    if (grouped)
        m_undoStack->endMacro();
    return changed;
}

int BoardScene::moveSelectedItemsToAdjacentLayer(int delta)
{
    if (delta == 0)
        return 0;

    QList<BoardItem*> targets;
    const QList<BoardItem*> selected = selectedBoardItems();
    for (BoardItem* item : selected) {
        if (item && isItemEditable(item))
            targets.append(item);
    }
    if (targets.isEmpty())
        return 0;

    const bool grouped = m_undoStack && targets.size() > 1;
    if (grouped)
        m_undoStack->beginMacro(delta > 0 ? uiText(QStringLiteral("Move to the layer above"))
                                         : uiText(QStringLiteral("Move to the layer below")));

    int changed = 0;
    for (BoardItem* item : targets) {
        if (moveItemToAdjacentLayer(item, delta))
            ++changed;
    }

    if (grouped)
        m_undoStack->endMacro();
    return changed;
}

void BoardScene::setPreferredLayerIds(const QList<int>& ids)
{
    if (m_preferredLayerIds == ids)
        return;
    m_preferredLayerIds = ids;
}

bool BoardScene::moveItemToAdjacentLayer(BoardItem* item, int delta, const QString& text)
{
    if (!item || delta == 0)
        return false;

    const int currentIndex = layerIndex(item->layerId());
    const int targetIndex = currentIndex + delta;
    if (currentIndex <= 0 || targetIndex <= 0 || targetIndex >= m_layers.size())
        return false;

    const int targetLayerId = m_layers.at(targetIndex).id;
    if (targetLayerId == item->layerId())
        return false;

    const ItemStackList before = itemStackSnapshot({ item });
    const int targetHeight = nextHeight(targetLayerId);
    if (targetHeight < 0)
        return false;
    const ItemStackList after = {
        { item, targetLayerId, targetHeight }
    };

    const QString commandText = text.isEmpty()
        ? (delta > 0 ? uiText(QStringLiteral("Move to the layer above")) : uiText(QStringLiteral("Move to the layer below")))
        : text;
    if (m_undoStack)
        m_undoStack->push(new ItemStackCommand(this, before, after, commandText));
    else
        applyItemStack(after);
    return true;
}

void BoardScene::applyItemStack(const ItemStackList& stack)
{
    QSet<int> affectedLayers;
    for (const ItemStack& entry : stack) {
        if (!entry.item)
            continue;
        if (!hasLayer(entry.layerId))
            entry.item->setLayerId(m_activeLayerId);
        else
            entry.item->setLayerId(entry.layerId);
        entry.item->setHeight(qBound(0, entry.height, kMaxItemHeight));
        affectedLayers.insert(entry.item->layerId());
        applyItemStacking(entry.item);
        entry.item->update();
    }
    for (int layerId : std::as_const(affectedLayers))
        normalizeLayerHeights(layerId);
    applyAllItemStacking();
    updateItemFlags();
    updateSceneExtent();
    update();
    emit contentChanged();
}

bool BoardScene::changeTextWithUndo(TextItem* item, const QString& text, const QFont& font,
                                    const QColor& color, const QString& textCommand)
{
    if (!item || item->scene() != this)
        return false;

    const TextItemState before = item->state();
    TextItemState after = before;
    after.text = text;
    after.font = font;
    after.color = color.isValid() ? color : before.color;
    after.baseSize = TextItem::measure(after.text, after.font);
    after.size = after.baseSize;

    if (before == after)
        return false;

    const QString commandText = textCommand.isEmpty()
        ? uiText(QStringLiteral("Edit text"))
        : textCommand;
    if (m_undoStack)
        m_undoStack->push(new TextItemCommand(this, item, before, after, commandText));
    else
        applyTextState(item, after);
    return true;
}

void BoardScene::applyTextState(TextItem* item, const TextItemState& state)
{
    if (!item)
        return;
    item->applyState(state);
    updateSceneExtent();
    update();
    emit contentChanged();
}

void BoardScene::assignNewItemPlacement(BoardItem* item)
{
    if (!item)
        return;

    if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId)
        item->setLayerId(m_activeLayerId);
    if (item->height() < 0 || item->height() > kMaxItemHeight
        || heightUsedByOther(item->layerId(), item->height(), item)) {
        const int height = nextHeight(item->layerId());
        item->setHeight(height >= 0 ? height : 0);
    }
    applyItemStacking(item);
}

void BoardScene::addItemsWithUndo(const QList<BoardItem*>& items, const QString& text)
{
    if (items.isEmpty())
        return;

    if (!hasLayer(m_activeLayerId))
        ensureLayerState();

    // Repair any externally supplied duplicate heights before reserving new
    // slots. Every layer must expose exactly one item per height value.
    normalizeAllHeights();

    // Assign the active layer and a unique height before the items are
    // attached, so a multi-image drop keeps the same top-to-bottom order it
    // had when it was created.
    QHash<int, QSet<int>> usedHeights;
    const QList<BoardItem*> existing = boardItems();
    for (const BoardItem* item : existing)
        usedHeights[item->layerId()].insert(item->height());

    for (BoardItem* item : items) {
        if (!item)
            continue;
        if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId)
            item->setLayerId(m_activeLayerId);

        QSet<int>& used = usedHeights[item->layerId()];
        int height = item->height();
        if (height < 0 || height > kMaxItemHeight || used.contains(height)) {
            height = nextHeight(item->layerId());
            while (height >= 0 && height < kMaxItemHeight && used.contains(height))
                ++height;
            if (height < 0 || used.contains(height))
                height = 0;
        }
        item->setHeight(qBound(0, height, kMaxItemHeight));
        used.insert(item->height());
        applyItemStacking(item);
    }

    if (m_undoStack) {
        if (items.size() == 1) {
            m_undoStack->push(new AddItemCommand(this, items.constFirst(), text));
        } else {
            m_undoStack->beginMacro(text);
            for (BoardItem* item : items)
                m_undoStack->push(new AddItemCommand(this, item, text));
            m_undoStack->endMacro();
        }
    } else {
        for (BoardItem* item : items)
            attachItem(item);
    }
    updateSceneExtent();
}

void BoardScene::commitNewItem(BoardItem* item, const QString& text)
{
    addItemsWithUndo({ item }, text);
}

QString BoardScene::uniqueItemName(const QString& base, const BoardItem* except) const
{
    const QString typeName = base.trimmed();
    const QString prefix = typeName + QLatin1Char(' ');
    int maxSuffix = 0;
    for (BoardItem* item : boardItems()) {
        if (!item || item == except)
            continue;
        const QString existing = item->rawName();
        if (!existing.startsWith(prefix))
            continue;
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0)
            maxSuffix = qMax(maxSuffix, value);
    }
    return QStringLiteral("%1 %2").arg(typeName).arg(maxSuffix + 1);
}

void BoardScene::assignAutoItemNames()
{
    for (BoardItem* item : boardItems()) {
        if (item && item->rawName().isEmpty())
            item->setName(uniqueItemName(item->typeDisplayName(), item));
    }
}

void BoardScene::attachItem(BoardItem* item)
{
    if (!item)
        return;

    m_orphans.removeAll(item);
    if (item->scene() != this)
        addItem(item);

    if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId)
        item->setLayerId(m_activeLayerId);
    if (item->height() < 0 || item->height() > kMaxItemHeight
        || heightUsedByOther(item->layerId(), item->height(), item)) {
        const int height = nextHeight(item->layerId());
        item->setHeight(height >= 0 ? height : 0);
    }
    if (item->rawName().isEmpty())
        item->setName(uniqueItemName(item->typeDisplayName(), item));
    applyItemStacking(item);
    applyItemInteraction(item);

    updateSceneExtent();
    update();
    emit contentChanged();
}

void BoardScene::detachItem(BoardItem* item)
{
    if (!item)
        return;

    if (item->scene() == this)
        removeItem(item);
    if (!m_orphans.contains(item))
        m_orphans.append(item);
    item->setSelected(false);

    updateSceneExtent();
    update();
    emit contentChanged();
}

void BoardScene::deleteSelected()
{
    QList<BoardItem*> selected;
    const QList<QGraphicsItem*> graphicsSelection = selectedItems();
    for (QGraphicsItem* graphicsItem : graphicsSelection) {
        if (auto* item = qgraphicsitem_cast<BoardItem*>(graphicsItem))
            selected.append(item);
    }
    if (selected.isEmpty())
        return;

    if (m_undoStack) {
        m_undoStack->push(new RemoveItemsCommand(this, selected, uiText(QStringLiteral("Delete item"))));
    } else {
        for (BoardItem* item : selected)
            detachItem(item);
    }
}

void BoardScene::selectAllItems()
{
    if (m_mode != ToolMode::Edit)
        return;
    const QList<BoardItem*> allItems = boardItems();
    for (BoardItem* item : allItems) {
        if (isItemEditable(item))
            item->setSelected(true);
    }
}

void BoardScene::pushGeometry(const ItemGeometryList& before, const ItemGeometryList& after,
                              const QString& text)
{
    if (before.size() != after.size())
        return;

    ItemGeometryList filteredBefore;
    ItemGeometryList filteredAfter;
    for (int i = 0; i < before.size(); ++i) {
        if (!before.at(i).item || !after.at(i).item)
            continue;
        if (sameGeometry(before.at(i), after.at(i)))
            continue;
        filteredBefore.append(before.at(i));
        filteredAfter.append(after.at(i));
    }
    if (filteredBefore.isEmpty())
        return;

    if (m_undoStack) {
        m_undoStack->push(new GeometryCommand(this, filteredBefore, filteredAfter, text));
    } else {
        for (const ItemGeometry& geometry : std::as_const(filteredAfter)) {
            if (!geometry.item)
                continue;
            geometry.item->setPos(geometry.pos);
            geometry.item->setContentSize(geometry.size);
        }
    }
    updateSceneExtent();
}

void BoardScene::resetDocument()
{
    if (m_undoStack)
        m_undoStack->clear();

    clear();
    clearOrphans();
    m_background = BackgroundState();
    m_layers = {
        { BackgroundLayerId, kBackgroundLayerName, true, true },
        { kFirstUserLayerId, QStringLiteral("图层 1"), true, false }
    };
    m_activeLayerId = kFirstUserLayerId;
    m_nextLayerId = kFirstUserLayerId + 1;
    m_boardOrigin = QPointF(0.0, 0.0);
    m_exporting = false;
    updateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    emit backgroundChanged();
}

void BoardScene::clearOrphans()
{
    qDeleteAll(m_orphans);
    m_orphans.clear();
}

BoardItem* BoardScene::createItem(const QString& kind) const
{
    if (kind == QStringLiteral("stroke"))
        return new StrokeItem();
    if (kind == QStringLiteral("image"))
        return new ImageItem();
    if (kind == QStringLiteral("text"))
        return new TextItem();
    if (kind == QStringLiteral("shape"))
        return new ShapeItem();
    return nullptr;
}

QJsonObject BoardScene::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("StoryBoardCanvas"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("generator"), QStringLiteral("StoryBoard 0.2"));

    QJsonObject canvas;
    canvas.insert(QStringLiteral("x"), m_boardOrigin.x());
    canvas.insert(QStringLiteral("y"), m_boardOrigin.y());
    canvas.insert(QStringLiteral("width"), m_boardSize.width());
    canvas.insert(QStringLiteral("height"), m_boardSize.height());
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

    QJsonArray layerArray;
    for (const BoardLayer& entry : m_layers) {
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
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items)
        itemsArray.append(item->toJson());
    root.insert(QStringLiteral("items"), itemsArray);

    return root;
}

bool BoardScene::fromJson(const QJsonObject& root, QString* errorMessage)
{
    auto fail = [errorMessage](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("StoryBoardCanvas"))
        return fail(uiText(QStringLiteral("This is not a valid StoryBoard canvas file.")));

    const QJsonObject canvas = root.value(QStringLiteral("canvas")).toObject();
    // 画布尺寸允许带小数（拖拽边缘时通常不是整数），这里不能用 toInt()。
    const qreal width = canvas.value(QStringLiteral("width")).toDouble(0.0);
    const qreal height = canvas.value(QStringLiteral("height")).toDouble(0.0);
    if (width <= 0.0 || height <= 0.0)
        return fail(uiText(QStringLiteral("The canvas size in the canvas file is invalid.")));

    resetDocument();
    applyBoardRect(QRectF(canvas.value(QStringLiteral("x")).toDouble(0.0),
                          canvas.value(QStringLiteral("y")).toDouble(0.0),
                          width, height));

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
            background.mode = backgroundModeFromString(
                backgroundJson.value(QStringLiteral("mode")).toString());
        }
    }
    applyBackground(background);

    const QJsonArray rawItems = root.value(QStringLiteral("items")).toArray();
    QList<QJsonObject> itemObjects;
    itemObjects.reserve(rawItems.size());
    for (const QJsonValue& value : rawItems)
        itemObjects.append(value.toObject());

    const QJsonArray layerArray = root.value(QStringLiteral("layers")).toArray();
    if (!root.value(QStringLiteral("layers")).isArray() || layerArray.isEmpty())
        return fail(uiText(QStringLiteral("This is not a valid StoryBoard canvas file.")));

    LayerStackState state;
    bool backgroundVisible = true;
    state.layers.append({ BackgroundLayerId, kBackgroundLayerName, true, true });
    QSet<int> used;
    used.insert(BackgroundLayerId);
    for (const QJsonValue& value : layerArray) {
        const QJsonObject layerJson = value.toObject();
        const int id = layerJson.value(QStringLiteral("id")).toInt();
        if (id == BackgroundLayerId) {
            backgroundVisible = layerJson.value(QStringLiteral("visible")).toBool(true);
            continue;
        }
        if (id <= 0 || used.contains(id))
            continue;
        BoardLayer entry;
        entry.id = id;
        entry.name = layerJson.value(QStringLiteral("name")).toString().trimmed();
        entry.visible = layerJson.value(QStringLiteral("visible")).toBool(true);
        entry.locked = layerJson.value(QStringLiteral("locked")).toBool(false);
        if (entry.name.isEmpty())
            entry.name = QStringLiteral("图层 %1").arg(id);
        state.layers.append(entry);
        used.insert(id);
    }
    state.layers[0].visible = backgroundVisible;
    state.activeLayerId = root.value(QStringLiteral("activeLayer")).toInt(kFirstUserLayerId);

    // 图元引用了未出现在图层数组中的图层时，补出该图层，避免图元丢失。
    for (const QJsonObject& itemJson : std::as_const(itemObjects)) {
        const int layerId = itemJson.value(QStringLiteral("layer")).toInt();
        if (layerId <= 0)
            continue;
        const bool exists = std::any_of(
            state.layers.cbegin(), state.layers.cend(),
            [layerId](const BoardLayer& entry) { return entry.id == layerId; });
        if (!exists)
            state.layers.append({ layerId, QStringLiteral("图层 %1").arg(layerId) });
    }

    applyLayerStateInternal(sanitizedLayerState(state), false);

    for (int i = 0; i < itemObjects.size(); ++i) {
        const QJsonObject itemJson = itemObjects.at(i);
        const QString kind = itemJson.value(QStringLiteral("type")).toString();
        BoardItem* item = createItem(kind);
        if (!item) {
            resetDocument();
            return fail(uiText(QStringLiteral("The canvas contains an unknown item type: %1")).arg(kind));
        }
        item->fromJson(itemJson);
        if (!hasLayer(item->layerId()) || item->layerId() == BackgroundLayerId) {
            item->setLayerId(m_activeLayerId);
        }
        addItem(item);
    }

    // 旧画布没有 itemName；补自动名称，保证重新保存后每个图元都有“类型 N”。
    assignAutoItemNames();

    // Assign heights for new-format items that omitted the property, then
    // normalize any duplicate heights in a layer while keeping their order.
    QHash<int, int> nextHeights;
    const QList<BoardItem*> items = boardItems();
    for (BoardItem* item : items) {
        if (item->height() >= 0)
            continue;
        auto it = nextHeights.constFind(item->layerId());
        int next = 0;
        if (it == nextHeights.constEnd()) {
            next = nextHeight(item->layerId());
            nextHeights.insert(item->layerId(), next);
        } else {
            next = it.value();
        }
        item->setHeight(qMin(kMaxItemHeight, next));
        nextHeights[item->layerId()] = qMin(kMaxItemHeight, next + 1);
    }

    // Enforce the canvas invariant: every layer has one item per height.
    // The stable sort inside normalizeLayerHeights preserves the stored order
    // when duplicate values are repaired.
    normalizeAllHeights();

    m_nextLayerId = 1;
    for (const BoardLayer& entry : std::as_const(m_layers))
        m_nextLayerId = qMax(m_nextLayerId, entry.id + 1);

    ensureLayerState();
    applyAllItemStacking();
    updateItemFlags();
    updateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    emit backgroundChanged();
    emit boardSizeChanged(m_boardSize);
    return true;
}

QImage BoardScene::renderImage(const QRectF& area, qreal scale)
{
    if (area.isEmpty() || scale <= 0.0)
        return QImage();

    QSizeF pixelSize(area.width() * scale, area.height() * scale);
    constexpr qreal kMaxSide = 12000.0;
    if (pixelSize.width() > kMaxSide || pixelSize.height() > kMaxSide) {
        const qreal factor = qMin(kMaxSide / pixelSize.width(), kMaxSide / pixelSize.height());
        pixelSize *= factor;
    }

    const QSize finalSize(qMax(1, qCeil(pixelSize.width())), qMax(1, qCeil(pixelSize.height())));
    QImage image(finalSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                           | QPainter::SmoothPixmapTransform);

    m_exporting = true;
    render(&painter,
           QRectF(0.0, 0.0, finalSize.width(), finalSize.height()),
           area,
           Qt::IgnoreAspectRatio);
    m_exporting = false;

    painter.end();
    return image;
}

QImage BoardScene::renderLayerImage(const QRectF& area, qreal scale, int layerId)
{
    if (!hasLayer(layerId))
        return QImage();

    const QList<BoardItem*> items = boardItems();
    QList<bool> previousVisibility;
    previousVisibility.reserve(items.size());
    for (BoardItem* item : items) {
        previousVisibility.append(item->isVisible());
        item->setVisible(item->layerId() == layerId && isLayerVisible(layerId));
    }

    QImage image = renderImage(area, scale);

    for (int i = 0; i < items.size() && i < previousVisibility.size(); ++i)
        items.at(i)->setVisible(previousVisibility.at(i));
    return image;
}

void BoardScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    const QRectF board = boardRect();
    const QRectF area = rect.intersected(board);
    if (area.isEmpty())
        return;

    painter->save();
    painter->setClipRect(rect);
    drawBoardBackground(painter, area, !m_exporting && m_showGrid);
    painter->restore();

    if (!m_exporting) {
        QPen borderPen(QColor(15, 23, 42, 55));
        borderPen.setCosmetic(true);
        borderPen.setWidthF(1.0);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(board.adjusted(0.5, 0.5, -0.5, -0.5));
    }
}

void BoardScene::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsScene::drawForeground(painter, rect);

    // 非查看模式下，被高层级/更高 height 图元遮住的图元补画一圈虚线轮廓，
    // 让用户仍能看出那里有东西（查看模式与导出不画）。
    const QList<BoardItem*> obscured = obscuredItems(rect);
    if (!painter || obscured.isEmpty())
        return;

    painter->save();
    painter->setClipRect(rect);
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 两层描边：先铺一圈半透明白色柔光，再压深色虚线。无论被遮住的是浅色
    // 画板还是深色图元，虚线轮廓都能看清。
    QPen haloPen(QColor(255, 255, 255, 120));
    haloPen.setWidthF(2.6);
    haloPen.setCosmetic(true);
    haloPen.setStyle(Qt::SolidLine);

    QPen dashPen(QColor(30, 41, 59, 205));
    dashPen.setWidthF(1.2);
    dashPen.setCosmetic(true);
    dashPen.setStyle(Qt::DashLine);
    dashPen.setDashPattern({ 5.0, 4.0 });

    for (BoardItem* item : obscured) {
        const QPainterPath path = item->sceneTransform().map(item->outlineShape());
        if (path.isEmpty())
            continue;
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
        painter->setPen(dashPen);
        painter->drawPath(path);
    }
    painter->restore();
}

void BoardScene::drawItems(QPainter* painter, int numItems, QGraphicsItem* items[],
                           const QStyleOptionGraphicsItem* options, QWidget* widget)
{
    if (numItems <= 1) {
        QGraphicsScene::drawItems(painter, numItems, items, options, widget);
        return;
    }

    // 遮挡必须是“完全不透”：被更高优先级图元涂满的地方，下层图元的颜色整个不画。
    // 直接按画家算法叠半透明颜色会混出第三种颜色，所以这里给每个图元算一块可见
    // 区域——从它上方所有图元涂满的区域里减掉，只画剩下的部分。
    // QGraphicsView 用 exposedRegion 过滤图元，画笔上通常没有设置裁剪；
    // 这种情况下 clipBoundingRect() 返回的是空矩形（不是整个设备），
    // 所以为空时自己取设备范围换算回场景坐标，否则会误判成“全部被遮住”。
    QRectF clipBounds = painter->clipBoundingRect();
    if (clipBounds.isEmpty()) {
        if (QPaintDevice* device = painter->device()) {
            clipBounds = painter->worldTransform().inverted().mapRect(
                QRectF(0, 0, device->width(), device->height()));
        }
    }
    const QRegion visibleBase(clipBounds.toAlignedRect());

    QList<int> order;
    order.reserve(numItems);
    for (int i = 0; i < numItems; ++i)
        order.append(i);
    std::stable_sort(order.begin(), order.end(), [items](int a, int b) {
        return items[a]->zValue() < items[b]->zValue();
    });

    QList<QRectF> bounds;
    bounds.reserve(order.size());
    for (int index : order)
        bounds.append(items[index]->sceneBoundingRect());

    // 上方图元的涂满区域按需计算：只有包围盒真的相交时才会用到。
    QList<QRegion> regions;
    regions.resize(order.size());
    QVector<bool> regionReady(order.size(), false);
    auto regionOf = [&](int position) -> const QRegion& {
        if (!regionReady.at(position)) {
            if (auto* boardItem = qgraphicsitem_cast<BoardItem*>(items[order.at(position)]))
                regions[position] = filledRegion(
                    boardItem->sceneTransform().map(boardItem->paintedShape()));
            else
                regions[position] = QRegion(bounds.at(position).toAlignedRect());
            regionReady[position] = true;
        }
        return regions.at(position);
    };

    for (int position = 0; position < order.size(); ++position) {
        QRegion covered;
        for (int above = position + 1; above < order.size(); ++above) {
            if (!bounds.at(above).intersects(bounds.at(position)))
                continue;
            covered = covered.united(regionOf(above));
        }

        const QRegion visible = visibleBase.subtracted(covered);
        if (visible.isEmpty())
            continue;

        const int index = order.at(position);
        painter->save();
        painter->setClipRegion(visible, Qt::IntersectClip);
        QGraphicsScene::drawItems(painter, 1, &items[index], &options[index], widget);
        painter->restore();
    }
}

void BoardScene::drawBoardBackground(QPainter* painter, const QRectF& area, bool includeGrid) const
{
    const QRectF board = boardRect();
    painter->fillRect(board, QColor(255, 255, 255));

    if (isLayerVisible(BackgroundLayerId)) {
        switch (m_background.type) {
        case BackgroundType::None:
            break;
        case BackgroundType::Color:
            painter->fillRect(board, m_background.color);
            break;
        case BackgroundType::Image: {
            const QPixmap& pixmap = m_background.image;
            if (pixmap.isNull())
                break;
            if (m_background.mode == BackgroundMode::Stretch) {
                painter->drawPixmap(board, pixmap, QRectF(pixmap.rect()));
            } else if (m_background.mode == BackgroundMode::Tile) {
                painter->drawTiledPixmap(board, pixmap);
            } else {
                const qreal factor = qMin(board.width() / pixmap.width(),
                                          board.height() / pixmap.height());
                QSizeF size(pixmap.width() * factor, pixmap.height() * factor);
                QRectF target(QPointF(0.0, 0.0), size);
                target.moveCenter(board.center());
                painter->drawPixmap(target, pixmap, QRectF(pixmap.rect()));
            }
            break;
        }
        }
    }

    if (!includeGrid)
        return;

    const qreal zoom = painter->worldTransform().m11();
    if (zoom < 0.18)
        return;
    const bool drawMinor = zoom >= 0.32;
    constexpr qreal step = 40.0;

    QPen minorPen(QColor(15, 23, 42, 13));
    minorPen.setCosmetic(true);
    minorPen.setWidthF(1.0);
    QPen majorPen(QColor(15, 23, 42, 30));
    majorPen.setCosmetic(true);
    majorPen.setWidthF(1.0);

    painter->save();
    painter->setClipRect(area);
    const int columns = qCeil(board.width() / step);
    for (int i = 0; i <= columns; ++i) {
        const bool major = (i % 5) == 0;
        if (!major && !drawMinor)
            continue;
        const qreal x = board.left() + i * step;
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(QPointF(x, board.top()), QPointF(x, board.bottom()));
    }
    const int rows = qCeil(board.height() / step);
    for (int i = 0; i <= rows; ++i) {
        const bool major = (i % 5) == 0;
        if (!major && !drawMinor)
            continue;
        const qreal y = board.top() + i * step;
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(QPointF(board.left(), y), QPointF(board.right(), y));
    }
    painter->restore();
}
