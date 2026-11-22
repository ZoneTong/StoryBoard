#pragma once

#include <QPoint>
#include <QPointF>
#include <QRectF>

class CanvasScene;
class CanvasView;
class QPainter;

// 画板边缘拖动拉伸 / 裁剪：命中八条边 / 角手柄、按手柄换算新矩形（带最小尺寸约束）、
// 悬停手柄光标与编辑模式下的手柄圆点 overlay。
class CanvasResizeController {
  public:
    enum class Handle { NONE, TOP_LEFT, TOP, TOP_RIGHT, RIGHT, BOTTOM_RIGHT, BOTTOM, BOTTOM_LEFT, LEFT };

    explicit CanvasResizeController(CanvasView* view, CanvasScene* scene);

    bool IsResizing() const { return m_resizingCanvas; }

    Handle HandleAt(const QPoint& viewportPos) const;
    static Qt::CursorShape HandleCursor(Handle handle);

    bool BeginResize(const QPoint& viewportPos);
    void UpdateResize(const QPointF& scenePos);
    void EndResize();
    void UpdateHoverCursor(const QPoint& viewportPos, bool panActive, bool strokeActive);
    void ResetHoverOnLeave();
    void DrawHandles(QPainter* painter);

  private:
    QRectF RectForDrag(const QPointF& scenePos) const;

    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
    bool m_resizingCanvas = false;
    Handle m_canvasHandle = Handle::NONE;
    Handle m_hoverHandle = Handle::NONE;
    QRectF m_resizeStartRect;
};
