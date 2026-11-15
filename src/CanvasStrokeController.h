#pragma once

#include <QColor>
#include <QPoint>
#include <QtGlobal>

class CanvasScene;
class CanvasView;
class CurveItem;

// 画笔交互：进行中的自由笔画、画笔颜色 / 粗细 / 收尾平滑容差，按下起笔、移动加点、
// 收尾提交或取消。图层锁定 / 隐藏时给出状态栏提示，不起笔。
class CanvasStrokeController {
  public:
    explicit CanvasStrokeController(CanvasView* view, CanvasScene* scene);

    QColor brushColor() const { return m_brushColor; }
    qreal brushWidth() const { return m_brushWidth; }
    int strokeSmoothing() const { return m_strokeSmoothing; }
    bool hasActiveStroke() const { return m_activeStroke != nullptr; }

    void setBrushColor(const QColor& color);
    void setBrushWidth(qreal width);
    void setStrokeSmoothing(int tolerance);

    // 起笔；图层不可写时发状态栏提示并返回 false。
    bool beginStroke(const QPoint& viewportPos);
    void appendPoint(const QPoint& viewportPos);
    void finishStroke();
    void cancelStroke();

  private:
    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
    CurveItem* m_activeStroke = nullptr;
    QColor m_brushColor = QColor(30, 41, 59);
    qreal m_brushWidth = 4.0;
    int m_strokeSmoothing = 4;
};
