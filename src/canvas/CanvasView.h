#pragma once

#include "canvas/CanvasViewCursor.h"
#include "canvas/CanvasViewHitTester.h"
#include "canvas/controllers/CanvasContextMenuController.h"
#include "canvas/controllers/CanvasGroupBoxController.h"
#include "canvas/controllers/CanvasPanController.h"
#include "canvas/controllers/CanvasResizeController.h"
#include "canvas/controllers/CanvasStrokeController.h"
#include "canvas/controllers/CanvasZoomController.h"
#include "items/CanvasTypes.h"

#include <QColor>
#include <QGraphicsView>
#include <QList>
#include <QPoint>
#include <memory>

class CanvasItem;
class CanvasScene;
class CanvasTextEditor;
class QAction;
class QContextMenuEvent;
class QFocusEvent;
class TextItem;

class CanvasView : public QGraphicsView {
    Q_OBJECT
  public:
    explicit CanvasView(CanvasScene* scene, QWidget* parent = nullptr);
    ~CanvasView() override;

    ToolMode mode() const { return m_mode; }
    void SetMode(ToolMode mode);

    qreal Zoom() const;
    // 控制器使用的显式入口：不再靠 friend 直接访问私有成员。
    CanvasViewHitTester* HitTester() { return m_hits.get(); }
    void RefreshCursor();
    void ShowDefaultContextMenu(QContextMenuEvent* event);
    // 缩放控制器需要把未消费的滚轮事件交回 QGraphicsView 的默认实现。
    void ForwardWheelEvent(QWheelEvent* event);
    void FitAll();
    void ZoomIn();
    void ZoomOut();
    void ActualSize();

    void SetBrushColor(const QColor& color);
    void SetBrushWidth(qreal width);
    QColor BrushColor() const { return m_stroke->BrushColor(); }
    qreal BrushWidth() const { return m_stroke->BrushWidth(); }
    // 画笔收尾自动平滑容差，单位为画布像素；0 表示关闭自动平滑。
    void SetStrokeSmoothing(qreal tolerance);
    qreal StrokeSmoothing() const { return m_stroke->StrokeSmoothing(); }

    void SetCanvasColor(const QColor& color);
    void CancelCurrentStroke();
    void FinishCurrentStroke();

    // 文字图元的内联编辑：新建文字后直接在图元上输入，焦点落在正文末尾。
    bool BeginTextEdit(TextItem* item);
    void EndTextEdit();
    bool IsTextEditing() const;

    // 画板边缘拖动：扩大或裁剪画板。返回当前是否正在拖动边缘。
    bool IsResizingCanvas() const { return m_resize->IsResizing(); }

    // 让主窗口把自己的动作组挂进右键菜单（例如布尔运算下拉），
    // 传进来的动作与菜单栏、工具栏共用同一个 QAction，状态自然同步。
    void AddContextMenuGroup(const QString& title, const QList<QAction*>& actions);
    int ContextMenuGroupCount() const { return m_menu->GroupCount(); }

  signals:
    void zoomChanged(qreal zoom);
    void modeChanged(ToolMode mode);
    void statusMessage(const QString& message);
    void textPlacementRequested(const QPointF& scenePos);

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
    void UpdateCursor();
    // 框选补判定：把整框落在选区内的图元选上（判定用图元实际外框，不含描边外扩）。
    void SelectItemsInsideBand(const QPoint& from, const QPoint& to);
    void BaseContextMenuEvent(QContextMenuEvent* event) { QGraphicsView::contextMenuEvent(event); }

    CanvasScene* m_scene = nullptr;
    ToolMode m_mode = ToolMode::VIEW;
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
    std::unique_ptr<CanvasGroupBoxController> m_groupBox;
    std::unique_ptr<CanvasTextEditor> m_textEditor;
};
