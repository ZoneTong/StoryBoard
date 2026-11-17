#pragma once

#include "CanvasContextMenuController.h"
#include "CanvasPanController.h"
#include "CanvasResizeController.h"
#include "CanvasStrokeController.h"
#include "CanvasTypes.h"
#include "CanvasViewCursor.h"
#include "CanvasViewHitTester.h"
#include "CanvasZoomController.h"

#include <QColor>
#include <QGraphicsView>
#include <QList>
#include <QPoint>
#include <memory>

class CanvasItem;
class CanvasScene;
class QAction;
class QContextMenuEvent;
class QFocusEvent;

class CanvasView : public QGraphicsView {
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
    QColor brushColor() const { return m_stroke->brushColor(); }
    qreal brushWidth() const { return m_stroke->brushWidth(); }
    // 画笔收尾自动平滑容差，单位为画布像素；0 表示关闭自动平滑。
    void setStrokeSmoothing(qreal tolerance);
    qreal strokeSmoothing() const { return m_stroke->strokeSmoothing(); }

    void setCanvasColor(const QColor& color);
    void cancelCurrentStroke();
    void finishCurrentStroke();

    // 画板边缘拖动：扩大或裁剪画板。返回当前是否正在拖动边缘。
    bool isResizingCanvas() const { return m_resize->isResizing(); }

    // 让主窗口把自己的动作组挂进右键菜单（例如布尔运算下拉），
    // 传进来的动作与菜单栏、工具栏共用同一个 QAction，状态自然同步。
    void addContextMenuGroup(const QString& title, const QList<QAction*>& actions);
    int contextMenuGroupCount() const { return m_menu->groupCount(); }

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
    void focusOutEvent(QFocusEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

  private:
    friend class CanvasZoomController;
    friend class CanvasPanController;
    friend class CanvasResizeController;
    friend class CanvasStrokeController;
    friend class CanvasContextMenuController;

    void updateCursor();
    // 框选补判定：把整框落在选区内的图元选上（判定用图元实际外框，不含描边外扩）。
    void selectItemsInsideBand(const QPoint& from, const QPoint& to);
    void baseContextMenuEvent(QContextMenuEvent* event) { QGraphicsView::contextMenuEvent(event); }

    CanvasScene* m_scene = nullptr;
    ToolMode m_mode = ToolMode::View;
    QColor m_canvasColor = QColor(43, 47, 54);
    bool m_spaceDown = false;
    // 空白处起手的框选：记下起点，松手时按图元实际外框补一次「整框包含」判定。
    // Qt 自带判定用的是含描边与手柄的 shape()，框线贴着图形时会被判为没框全。
    bool m_bandCandidate = false;
    QPoint m_bandOrigin;

    std::unique_ptr<CanvasZoomController> m_zoom;
    std::unique_ptr<CanvasPanController> m_pan;
    std::unique_ptr<CanvasResizeController> m_resize;
    std::unique_ptr<CanvasStrokeController> m_stroke;
    std::unique_ptr<CanvasViewHitTester> m_hits;
    std::unique_ptr<CanvasViewCursor> m_cursor;
    std::unique_ptr<CanvasContextMenuController> m_menu;
};
