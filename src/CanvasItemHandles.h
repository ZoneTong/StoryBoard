#pragma once

#include <QCursor>
#include <QPointF>
#include <QRectF>

class CanvasItem;
class QPainter;
class QPainterPath;

// 选中手柄的几何、命中、光标与绘制：八个缩放手柄加一个旋转手柄。
// 几何依据图元的内容矩形与当前视图缩放计算，本身不保存状态。
class CanvasItemHandles {
  public:
    explicit CanvasItemHandles(CanvasItem* item);

    qreal size() const;
    QPointF center(int handle) const;
    QRectF rect(int handle) const;
    int hitAt(const QPointF& pos) const;
    qreal rotationOffset() const;

    // 命中测试用的手柄区域（含手柄半露在图元几何外的部分）。
    QPainterPath selectionShape() const;
    void drawOverlay(QPainter* painter) const;
    QCursor cursor(int handle) const;

    // 需求 2：单选三角形时顶点 C 取代 Top 句柄（类内才能访问受保护的单选判定）。
    bool usesApexHandle() const;

  private:
    CanvasItem* m_item = nullptr;
};
