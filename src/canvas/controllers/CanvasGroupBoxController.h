#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <Qt>

class CanvasScene;
class CanvasView;
class CanvasItem;
class QPainter;

// 组选框手柄：八向缩放 + 顶部旋转钮；BODY 只命中不拖动（整组移动走默认图元拖拽）。
enum class GroupBoxHandle {
    NONE = -1,
    BODY = 0,
    NW = 1,
    N = 2,
    NE = 3,
    E = 4,
    SE = 5,
    S = 6,
    SW = 7,
    W = 8,
    ROTATE = 9
};

// 组选框（§7.6/§7.7）：选中恰好是一个完整最外层组时，由视图层统一画组框；
// 组框缩放 / 旋转只改成员几何，不落任何组级几何字段，组旋转不写进文档。
class CanvasGroupBoxController {
  public:
    CanvasGroupBoxController(CanvasView* view, CanvasScene* scene);

    bool Active() const;
    int GroupId() const;
    QRectF GroupRect() const;

    int HitTest(const QPoint& viewportPos) const;
    void BeginDrag(const QPoint& viewportPos, int handle);
    void UpdateDrag(const QPoint& viewportPos, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void EndDrag();
    bool IsDragging() const { return m_dragging; }

    void DrawOverlay(QPainter* painter) const;

  private:
    struct MemberSnapshot {
        CanvasItem* item = nullptr;
        QPointF oldPos;
        QSizeF oldSize;
        qreal oldRotation = 0.0;
    };

    QList<CanvasItem*> Members() const;
    QList<MemberSnapshot> Snapshot() const;
    // 组内存在旋转成员时，轴向量缩放会产生剪切 → 只保留角手柄（等比）。
    bool HasRotatedMember() const;
    QRectF ResizeRect(int handle, const QRectF& startRect, const QPointF& scenePos, bool equalize) const;
    QPointF HandleCenter(int handle, const QRectF& rect) const;
    QRectF ViewportRect() const;
    static QPointF RotateAround(const QPointF& point, const QPointF& center, qreal angle);

    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
    bool m_dragging = false;
    int m_handle = static_cast<int>(GroupBoxHandle::NONE);
    QRectF m_startRect;
    QPointF m_rotateCenter;
    qreal m_startAngle = 0.0;
    QList<MemberSnapshot> m_snapshot;
};
