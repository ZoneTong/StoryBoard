#pragma once

#include <QtGlobal>

class CanvasScene;
class CanvasView;
class QWheelEvent;

// 视图缩放与适配状态：缩放档位限制、适配全图、实际大小、滚轮以鼠标为锚点缩放。
// 只做视图变换换算，不处理图元选择、画笔或平移交互。
class CanvasZoomController {
  public:
    explicit CanvasZoomController(CanvasView* view, CanvasScene* scene);

    qreal zoom() const;
    void fitAll();
    void zoomIn();
    void zoomOut();
    void actualSize();
    void applyScale(qreal factor);
    void emitZoom();
    void handleWheel(QWheelEvent* event);

  private:
    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
};
