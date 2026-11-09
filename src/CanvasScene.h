#pragma once

#include <QGraphicsScene>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QSizeF>

#include "CanvasTypes.h"
#include "ItemGeometry.h"

class QUndoStack;
class CanvasItem;
class QFont;
class QPainter;
class QStyleOptionGraphicsItem;
class TextItem;
struct TextItemState;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit CanvasScene(QObject* parent = nullptr);
    ~CanvasScene() override;

    void setUndoStack(QUndoStack* stack);
    QUndoStack* undoStack() const { return m_undoStack; }

    ToolMode mode() const { return m_mode; }
    void setMode(ToolMode mode);

    QSizeF canvasSize() const { return m_canvasSize; }
    QPointF canvasOrigin() const { return m_canvasOrigin; }
    QRectF canvasRect() const;
    void applyCanvasSize(const QSizeF& size);
    void setCanvasSizeWithUndo(const QSizeF& size);
    // Canvas geometry includes the origin so the canvas can be stretched from
    // any edge; apply* is live (drag) and commit* pushes one undo step.
    void applyCanvasRect(const QRectF& rect);
    void setCanvasRectWithUndo(const QRectF& rect);
    void commitCanvasRect(const QRectF& before);

    BackgroundState background() const { return m_background; }
    void applyBackground(const BackgroundState& background);
    void replaceBackground(const BackgroundState& background, const QString& text = QString());

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool enabled);

    QRectF contentRect() const;
    void updateSceneExtent();
    // 视图缩放后刷新图元的 boundingRect（旋转手柄等屏幕尺寸相关几何）。
    void refreshViewDependentGeometry();

    QList<CanvasItem*> canvasItems() const;
    int canvasItemCount() const;
    // 需求 32：给所有还没有自定义名称的图元补上“类型 1 / 类型 2 …”自动名称。
    // 同时修复载入文件里已有的重名条目，保证画布内名称唯一。
    void assignAutoItemNames();
    QList<CanvasItem*> selectedCanvasItems() const;
    // 名称只按画布内图元的 name() 比较；空名称不占用唯一名额。
    bool isItemNameAvailable(const QString& name, const CanvasItem* except = nullptr) const;
    // 按“类型 1 / 类型 2 …”生成类型专属的自动名称；用于新建图元与空名称补名。
    QString uniqueItemName(const QString& base, const CanvasItem* except = nullptr) const;
    // 在已有名称基础上生成唯一副本名称：Hero panel → Hero panel 2，矩形 1 → 矩形 2。
    QString uniqueItemNameFor(const QString& requested, const CanvasItem* except = nullptr) const;
    // 复制当前选中的可编辑图元（同层、偏移一点、名称自动取唯一值），整体一条撤销记录。
    // 返回实际生成的副本数量。
    int duplicateSelectedItems();

    // 分组：只允许同层、至少两个可编辑图元。移动任意组员时整组移动，
    // 但组员仍可单独选中并编辑属性。
    bool canGroupSelectedItems() const;
    bool canUngroupSelectedItems() const;
    int groupSelectedItems();
    int ungroupSelectedItems();
    QList<CanvasItem*> itemsInGroup(int groupId) const;
    void applyGroupIds(const QList<CanvasItem*>& items, const QList<int>& groupIds);

    static constexpr int BackgroundLayerId = 0;

    const QList<CanvasLayer>& layers() const { return m_layers; }
    QList<CanvasLayer> userLayers() const;
    int layerCount() const { return m_layers.size(); }
    int userLayerCount() const { return qMax(0, m_layers.size() - 1); }
    bool hasLayer(int layerId) const;
    CanvasLayer layer(int layerId) const;
    int layerIndex(int layerId) const;
    int layerItemCount(int layerId) const;
    QList<CanvasItem*> layerItems(int layerId) const;
    bool isLayerVisible(int layerId) const;
    bool setLayerVisible(int layerId, bool visible);
    bool setAllLayersVisible(bool visible);
    bool isLayerLocked(int layerId) const;
    bool setLayerLocked(int layerId, bool locked);
    bool isItemEditable(const CanvasItem* item) const;
    // True when the active layer accepts new items (exists, visible, unlocked).
    bool isActiveLayerWritable() const;
    int activeLayerId() const { return m_activeLayerId; }
    void setActiveLayer(int layerId);

    int createLayer(const QString& name = QString());
    bool addLayerWithUndo(const QString& name = QString());
    bool removeLayerWithUndo(int layerId, QString* errorMessage = nullptr);
    bool moveLayerWithUndo(int layerId, int delta);
    bool renameLayerWithUndo(int layerId, const QString& name);
    LayerStackState layerState() const;
    void applyLayerState(const LayerStackState& state);

    int nextHeight(int layerId) const;
    bool layerHeightsUnique(int layerId) const;
    bool adjustItemHeight(CanvasItem* item, int delta, const QString& text = QString());
    bool moveItemToAdjacentLayer(CanvasItem* item, int delta, const QString& text = QString());
    // 对当前选中的所有图元批量调整高度 / 图层，整体只占一条撤销记录。
    int adjustSelectedHeights(int delta);
    int moveSelectedItemsToAdjacentLayer(int delta);

    // 图层面板中被选中的图层。重叠图元命中时优先选择这些图层里的图元。
    void setPreferredLayerIds(const QList<int>& ids);
    QList<int> preferredLayerIds() const { return m_preferredLayerIds; }
    void applyItemStack(const ItemStackList& stack);

    bool changeTextWithUndo(TextItem* item, const QString& text, const QFont& font,
                            const QColor& color, const QString& commandText = QString());
    void applyTextState(TextItem* item, const TextItemState& state);

    void addItemsWithUndo(const QList<CanvasItem*>& items, const QString& text);
    void commitNewItem(CanvasItem* item, const QString& text);
    void attachItem(CanvasItem* item);
    void detachItem(CanvasItem* item);
    void deleteSelected();
    void selectAllItems();
    void pushGeometry(const ItemGeometryList& before, const ItemGeometryList& after,
                      const QString& text);
    // 需求 35：旋转手柄与快捷键的撤销入口。
    void pushRotation(CanvasItem* item, qreal before, qreal after, const QString& text);
    int rotateSelected(qreal degrees, const QString& commandText = QString());

    void resetDocument();
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& root, QString* errorMessage = nullptr);

    QImage renderImage(const QRectF& area, qreal scale);
    // 只渲染指定图层上的图元；画板背景仍按背景层的显示状态保留。
    QImage renderLayerImage(const QRectF& area, qreal scale, int layerId);
    bool isExporting() const { return m_exporting; }

    // 非查看模式下被更高优先级图元遮住、需要额外描出轮廓的图元。
    // 优先级：图层越高越优先；同一图层内 height 越大越优先。
    // area 非空时只统计与该区域相交的图元（绘制时用它避免整场景重算）。
    QList<CanvasItem*> obscuredItems(const QRectF& area = QRectF()) const;

signals:
    void modeChanged(ToolMode mode);
    void contentChanged();
    void backgroundChanged();
    void canvasSizeChanged(const QSizeF& size);
    void layersChanged();
    void activeLayerChanged(int layerId);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    // 逐图元裁剪：被更高优先级图元盖住的部分不画下层颜色（不是半透明混合）。
    void drawItems(QPainter* painter, int numItems, QGraphicsItem* items[],
                   const QStyleOptionGraphicsItem* options, QWidget* widget = nullptr) override;

private:
    CanvasItem* createItem(const QString& kind) const;
    // reserved 里是尚未挂到场景、但已经保留给其它副本的名称。
    QString uniqueItemNameFor(const QString& requested, const CanvasItem* except,
                              const QSet<QString>& reserved) const;
    void drawCanvasBackground(QPainter* painter, const QRectF& area, bool includeGrid) const;
    void updateItemFlags();
    void applyItemInteraction(CanvasItem* item);
    void clearOrphans();

    void ensureLayerState();
    LayerStackState sanitizedLayerState(const LayerStackState& state) const;
    void applyLayerStateInternal(const LayerStackState& state, bool preserveLock = true);
    void assignNewItemPlacement(CanvasItem* item);
    void applyItemStacking(CanvasItem* item);
    void applyAllItemStacking();
    bool normalizeLayerHeights(int layerId);
    void normalizeAllHeights();
    bool heightUsedByOther(int layerId, int height, const CanvasItem* except) const;
    qreal layerBaseZ(int layerId) const;
    ItemStackList itemStackSnapshot(const QList<CanvasItem*>& items) const;

    QUndoStack* m_undoStack = nullptr;
    ToolMode m_mode = ToolMode::View;
    QSizeF m_canvasSize { 1920.0, 1080.0 };
    QPointF m_canvasOrigin { 0.0, 0.0 };
    BackgroundState m_background;
    bool m_showGrid = true;
    bool m_exporting = false;
    QList<CanvasLayer> m_layers;
    int m_activeLayerId = 1;
    int m_nextLayerId = 2;
    int m_nextGroupId = 1;
    QList<int> m_preferredLayerIds;
    QList<CanvasItem*> m_orphans;
};
