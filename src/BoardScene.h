#pragma once

#include <QGraphicsScene>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QSizeF>

#include "BoardTypes.h"
#include "ItemGeometry.h"

class QUndoStack;
class BoardItem;
class QFont;
class QPainter;
class QStyleOptionGraphicsItem;
class TextItem;
struct TextItemState;

class BoardScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit BoardScene(QObject* parent = nullptr);
    ~BoardScene() override;

    void setUndoStack(QUndoStack* stack);
    QUndoStack* undoStack() const { return m_undoStack; }

    ToolMode mode() const { return m_mode; }
    void setMode(ToolMode mode);

    QSizeF boardSize() const { return m_boardSize; }
    QPointF boardOrigin() const { return m_boardOrigin; }
    QRectF boardRect() const;
    void applyBoardSize(const QSizeF& size);
    void setBoardSizeWithUndo(const QSizeF& size);
    // Board geometry includes the origin so the canvas can be stretched from
    // any edge; apply* is live (drag) and commit* pushes one undo step.
    void applyBoardRect(const QRectF& rect);
    void setBoardRectWithUndo(const QRectF& rect);
    void commitBoardRect(const QRectF& before);

    BackgroundState background() const { return m_background; }
    void applyBackground(const BackgroundState& background);
    void replaceBackground(const BackgroundState& background, const QString& text = QString());

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool enabled);

    QRectF contentRect() const;
    void updateSceneExtent();

    QList<BoardItem*> boardItems() const;
    int boardItemCount() const;
    QList<BoardItem*> selectedBoardItems() const;

    static constexpr int BackgroundLayerId = 0;

    const QList<BoardLayer>& layers() const { return m_layers; }
    QList<BoardLayer> userLayers() const;
    int layerCount() const { return m_layers.size(); }
    int userLayerCount() const { return qMax(0, m_layers.size() - 1); }
    bool hasLayer(int layerId) const;
    BoardLayer layer(int layerId) const;
    int layerIndex(int layerId) const;
    int layerItemCount(int layerId) const;
    QList<BoardItem*> layerItems(int layerId) const;
    bool isLayerVisible(int layerId) const;
    bool setLayerVisible(int layerId, bool visible);
    bool setAllLayersVisible(bool visible);
    bool isLayerLocked(int layerId) const;
    bool setLayerLocked(int layerId, bool locked);
    bool isItemEditable(const BoardItem* item) const;
    // True when the active layer accepts new items (exists, visible, unlocked).
    bool isActiveLayerWritable() const;
    int activeLayerId() const { return m_activeLayerId; }
    void setActiveLayer(int layerId);

    int createLayer(const QString& name = QString());
    bool addLayerWithUndo(const QString& name = QString());
    bool removeLayerWithUndo(int layerId, QString* errorMessage = nullptr);
    bool moveLayerWithUndo(int layerId, int delta);
    LayerStackState layerState() const;
    void applyLayerState(const LayerStackState& state);

    int nextHeight(int layerId) const;
    bool layerHeightsUnique(int layerId) const;
    bool adjustItemHeight(BoardItem* item, int delta, const QString& text = QString());
    bool moveItemToAdjacentLayer(BoardItem* item, int delta, const QString& text = QString());
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

    void addItemsWithUndo(const QList<BoardItem*>& items, const QString& text);
    void commitNewItem(BoardItem* item, const QString& text);
    void attachItem(BoardItem* item);
    void detachItem(BoardItem* item);
    void deleteSelected();
    void selectAllItems();
    void pushGeometry(const ItemGeometryList& before, const ItemGeometryList& after,
                      const QString& text);

    void resetDocument();
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& root, QString* errorMessage = nullptr);

    QImage renderImage(const QRectF& area, qreal scale);
    bool isExporting() const { return m_exporting; }

    // 非查看模式下被更高优先级图元遮住、需要额外描出轮廓的图元。
    // 优先级：图层越高越优先；同一图层内 height 越大越优先。
    // area 非空时只统计与该区域相交的图元（绘制时用它避免整场景重算）。
    QList<BoardItem*> obscuredItems(const QRectF& area = QRectF()) const;

signals:
    void modeChanged(ToolMode mode);
    void contentChanged();
    void backgroundChanged();
    void boardSizeChanged(const QSizeF& size);
    void layersChanged();
    void activeLayerChanged(int layerId);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    // 逐图元裁剪：被更高优先级图元盖住的部分不画下层颜色（不是半透明混合）。
    void drawItems(QPainter* painter, int numItems, QGraphicsItem* items[],
                   const QStyleOptionGraphicsItem* options, QWidget* widget = nullptr) override;

private:
    BoardItem* createItem(const QString& kind) const;
    void drawBoardBackground(QPainter* painter, const QRectF& area, bool includeGrid) const;
    void updateItemFlags();
    void applyItemInteraction(BoardItem* item);
    void clearOrphans();

    void ensureLayerState();
    LayerStackState sanitizedLayerState(const LayerStackState& state) const;
    void applyLayerStateInternal(const LayerStackState& state, bool preserveLock = true);
    void assignNewItemPlacement(BoardItem* item);
    void applyItemStacking(BoardItem* item);
    void applyAllItemStacking();
    bool normalizeLayerHeights(int layerId);
    void normalizeAllHeights();
    bool heightUsedByOther(int layerId, int height, const BoardItem* except) const;
    qreal layerBaseZ(int layerId) const;
    ItemStackList itemStackSnapshot(const QList<BoardItem*>& items) const;

    QUndoStack* m_undoStack = nullptr;
    ToolMode m_mode = ToolMode::View;
    QSizeF m_boardSize { 1920.0, 1080.0 };
    QPointF m_boardOrigin { 0.0, 0.0 };
    BackgroundState m_background;
    bool m_showGrid = true;
    bool m_exporting = false;
    QList<BoardLayer> m_layers;
    int m_activeLayerId = 1;
    int m_nextLayerId = 2;
    QList<int> m_preferredLayerIds;
    QList<BoardItem*> m_orphans;
};
