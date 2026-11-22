#pragma once

#include "HandleSpec.h"

#include <QCursor>
#include <QPointF>
#include <QRectF>

class CanvasItem;
class QPainter;
class QPainterPath;

// 选中手柄的几何、命中、光标与绘制：八个缩放手柄、一个旋转手柄，以及图元用
// CustomHandles() 声明的自定义句柄。自定义句柄的位置、行为、光标与尺寸都来自
// HandleSpec（类内才能访问受保护的单选判定），本类不再按图元类型分支。
// 几何依据图元的内容矩形与当前视图缩放计算，本身不保存状态。
class CanvasItemHandles {
  public:
    explicit CanvasItemHandles(CanvasItem* item);

    qreal Size() const;
    QPointF Center(int handle) const;
    QRectF Rect(int handle) const;
    int HitAt(const QPointF& pos) const;
    qreal RotationOffset() const;

    // 命中测试用的手柄区域（含手柄半露在图元几何外的部分）。
    QPainterPath SelectionShape() const;
    void DrawOverlay(QPainter* painter) const;
    QCursor Cursor(int handle) const;

  private:
    // 当前生效的自定义句柄：只有单选、可编辑时图元才提供，否则一律没有。
    HandleSpecList ActiveSpecs() const;
    // 按句柄槽位取自定义句柄描述（返回值拷贝，避免悬垂引用）。
    bool SpecAt(int handle, HandleSpec* spec) const;

    CanvasItem* m_item = nullptr;
};
