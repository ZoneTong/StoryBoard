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
    QRectF boardRect() const;
    void applyBoardSize(const QSizeF& size);
    void setBoardSizeWithUndo(const QSizeF& size);

    BackgroundState background() const { return m_background; }
    void applyBackground(const BackgroundState& background);
    void replaceBackground(const BackgroundState& background, const QString& text = QString());

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool enabled);

    QRectF contentRect() const;
    void updateSceneExtent();

    QList<BoardItem*> boardItems() const;
    int boardItemCount() const;

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
    bool isItemEditable(const BoardItem* item) const;
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

signals:
    void modeChanged(ToolMode mode);
    void contentChanged();
    void backgroundChanged();
    void boardSizeChanged(const QSizeF& size);
    void layersChanged();
    void activeLayerChanged(int layerId);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    BoardItem* createItem(const QString& kind) const;
    void drawBoardBackground(QPainter* painter, const QRectF& area, bool includeGrid) const;
    void updateItemFlags();
    void applyItemInteraction(BoardItem* item);
    void clearOrphans();

    void ensureLayerState();
    LayerStackState sanitizedLayerState(const LayerStackState& state) const;
    void applyLayerStateInternal(const LayerStackState& state);
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
    BackgroundState m_background;
    bool m_showGrid = true;
    bool m_exporting = false;
    QList<BoardLayer> m_layers;
    int m_activeLayerId = 1;
    int m_nextLayerId = 2;
    QList<BoardItem*> m_orphans;
};
