#pragma once

#include <QColor>
#include <QGraphicsView>

#include "BoardTypes.h"

class BoardScene;
class StrokeItem;
class TextItem;
class QContextMenuEvent;

class BoardView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit BoardView(BoardScene* scene, QWidget* parent = nullptr);

    ToolMode mode() const { return m_mode; }
    void setMode(ToolMode mode);

    qreal zoom() const;
    void fitAll();
    void zoomIn();
    void zoomOut();
    void actualSize();

    void setBrushColor(const QColor& color);
    void setBrushWidth(qreal width);
    QColor brushColor() const { return m_brushColor; }
    qreal brushWidth() const { return m_brushWidth; }

    void setCanvasColor(const QColor& color);
    void cancelCurrentStroke();
    void finishCurrentStroke();

signals:
    void zoomChanged(qreal zoom);
    void modeChanged(ToolMode mode);
    void statusMessage(const QString& message);
    void textPlacementRequested(const QPointF& scenePos);
    void editTextRequested(TextItem* item);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void applyScale(qreal factor);
    void emitZoom();
    void updateCursor();
    void beginPan(const QPoint& position);
    void endPan();

    BoardScene* m_scene = nullptr;
    ToolMode m_mode = ToolMode::View;
    StrokeItem* m_activeStroke = nullptr;
    QColor m_brushColor = QColor(30, 41, 59);
    QColor m_canvasColor = QColor(43, 47, 54);
    qreal m_brushWidth = 4.0;
    bool m_spaceDown = false;
    bool m_panning = false;
    QPoint m_panStart;
    QPointF m_panAnchorScene;
    QPointF m_panCenterScene;
};
