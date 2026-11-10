#pragma once

#include <QColor>
#include <QGraphicsView>

#include "CanvasTypes.h"

class CanvasItem;
class CanvasScene;
class StrokeItem;
class QContextMenuEvent;

class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CanvasView(CanvasScene* scene, QWidget* parent = nullptr);

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
    // 画笔收尾自动平滑容差，单位为画布像素；0 表示关闭自动平滑。
    void setStrokeSmoothing(int tolerance);
    int strokeSmoothing() const { return m_strokeSmoothing; }

    void setCanvasColor(const QColor& color);
    void cancelCurrentStroke();
    void finishCurrentStroke();

    // 画板边缘拖动：扩大或裁剪画板。返回当前是否正在拖动边缘。
    bool isResizingCanvas() const { return m_resizingCanvas; }

signals:
    void zoomChanged(qreal zoom);
    void modeChanged(ToolMode mode);
    void statusMessage(const QString& message);
    void textPlacementRequested(const QPointF& scenePos);
    void itemPropertiesRequested(CanvasItem* item);

protected:
    void paintEvent(QPaintEvent* event) override;
    void leaveEvent(QEvent* event) override;
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

    enum class CanvasHandle {
        None,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left
    };

    CanvasItem* topCanvasItemAt(const QPoint& viewportPos) const;
    // 需求 42：属性窗在所有模式可查看，因此命中图元不能依赖编辑模式。
    CanvasItem* propertyItemAt(const QPoint& viewportPos) const;
    CanvasItem* editableItemAt(const QPoint& viewportPos) const;
    CanvasItem* preferredEditableItemAt(const QPoint& viewportPos) const;
    CanvasHandle canvasHandleAt(const QPoint& viewportPos) const;
    bool beginCanvasResize(const QPoint& viewportPos);
    void updateCanvasResize(const QPointF& scenePos);
    void endCanvasResize();
    void updateCanvasHoverCursor(const QPoint& viewportPos);
    QRectF canvasRectForDrag(const QPointF& scenePos) const;
    void drawCanvasHandles(QPainter* painter);
    static Qt::CursorShape canvasHandleCursor(CanvasHandle handle);

    CanvasScene* m_scene = nullptr;
    ToolMode m_mode = ToolMode::View;
    StrokeItem* m_activeStroke = nullptr;
    QColor m_brushColor = QColor(30, 41, 59);
    QColor m_canvasColor = QColor(43, 47, 54);
    qreal m_brushWidth = 4.0;
    int m_strokeSmoothing = 4;
    bool m_spaceDown = false;
    bool m_panning = false;
    QPoint m_panStart;
    QPointF m_panCenterScene;

    bool m_resizingCanvas = false;
    CanvasHandle m_canvasHandle = CanvasHandle::None;
    CanvasHandle m_hoverHandle = CanvasHandle::None;
    QRectF m_resizeStartRect;
};
