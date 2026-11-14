#pragma once

#include <QPoint>
#include <QPointF>

class CanvasView;

// 按住空格 / 中键 / 查看模式左键拖动平移：记录按下锚点，按“按下点 → 当前点”的
// 场景位移居中，避免视图移动后重复计入位移造成画面抖动。
class CanvasPanController {
  public:
    explicit CanvasPanController(CanvasView* view);

    bool isPanning() const { return m_panning; }

    void beginPan(const QPoint& position);
    void endPan();
    // 平移进行中则按当前点更新视图并返回 true；否则返回 false。
    bool movePan(const QPoint& position);

  private:
    CanvasView* m_view = nullptr;
    bool m_panning = false;
    bool m_panMoved = false;
    QPoint m_panStart;
    QPointF m_panCenterScene;
};
