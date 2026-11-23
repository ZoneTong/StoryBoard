#include "canvas/controllers/CanvasResizeController.h"

#include "canvas/CanvasView.h"
#include "scene/CanvasScene.h"

#include <QCoreApplication>
#include <QPainter>
#include <QtMath>

namespace {
QString TrView(const char* text)
{
    return QCoreApplication::translate("CanvasView", text);
}
} // namespace

CanvasResizeController::CanvasResizeController(CanvasView* view, CanvasScene* scene) : m_view(view), m_scene(scene) {}

Qt::CursorShape CanvasResizeController::HandleCursor(Handle handle)
{
    switch (handle) {
    case Handle::LEFT:
    case Handle::RIGHT:
        return Qt::SizeHorCursor;
    case Handle::TOP:
    case Handle::BOTTOM:
        return Qt::SizeVerCursor;
    case Handle::TOP_LEFT:
    case Handle::BOTTOM_RIGHT:
        return Qt::SizeFDiagCursor;
    case Handle::TOP_RIGHT:
    case Handle::BOTTOM_LEFT:
        return Qt::SizeBDiagCursor;
    case Handle::NONE:
        break;
    }
    return Qt::ArrowCursor;
}

CanvasResizeController::Handle CanvasResizeController::HandleAt(const QPoint& viewportPos) const
{
    if (m_view->mode() != ToolMode::EDIT) {
        return Handle::NONE;
    }

    const QRectF rect = m_view->mapFromScene(m_scene->CanvasRect()).boundingRect();
    const QPointF point = viewportPos;
    const qreal tolerance = 7.0;
    const QRectF outer = rect.adjusted(-tolerance, -tolerance, tolerance, tolerance);
    if (!outer.contains(point)) {
        return Handle::NONE;
    }

    const bool left = qAbs(point.x() - rect.left()) <= tolerance;
    const bool right = qAbs(point.x() - rect.right()) <= tolerance;
    const bool top = qAbs(point.y() - rect.top()) <= tolerance;
    const bool bottom = qAbs(point.y() - rect.bottom()) <= tolerance;

    if (left && top) {
        return Handle::TOP_LEFT;
    }
    if (right && top) {
        return Handle::TOP_RIGHT;
    }
    if (left && bottom) {
        return Handle::BOTTOM_LEFT;
    }
    if (right && bottom) {
        return Handle::BOTTOM_RIGHT;
    }
    if (left) {
        return Handle::LEFT;
    }
    if (right) {
        return Handle::RIGHT;
    }
    if (top) {
        return Handle::TOP;
    }
    if (bottom) {
        return Handle::BOTTOM;
    }
    return Handle::NONE;
}

bool CanvasResizeController::BeginResize(const QPoint& viewportPos)
{
    const Handle handle = HandleAt(viewportPos);
    if (handle == Handle::NONE) {
        return false;
    }

    m_canvasHandle = handle;
    m_resizingCanvas = true;
    m_resizeStartRect = m_scene->CanvasRect();
    m_view->viewport()->setCursor(HandleCursor(handle));
    return true;
}

QRectF CanvasResizeController::RectForDrag(const QPointF& scenePos) const
{
    QRectF rect = m_resizeStartRect;
    switch (m_canvasHandle) {
    case Handle::LEFT:
        rect.setLeft(scenePos.x());
        break;
    case Handle::RIGHT:
        rect.setRight(scenePos.x());
        break;
    case Handle::TOP:
        rect.setTop(scenePos.y());
        break;
    case Handle::BOTTOM:
        rect.setBottom(scenePos.y());
        break;
    case Handle::TOP_LEFT:
        rect.setTopLeft(scenePos);
        break;
    case Handle::TOP_RIGHT:
        rect.setTopRight(scenePos);
        break;
    case Handle::BOTTOM_LEFT:
        rect.setBottomLeft(scenePos);
        break;
    case Handle::BOTTOM_RIGHT:
        rect.setBottomRight(scenePos);
        break;
    case Handle::NONE:
        return rect;
    }

    rect = rect.normalized();
    constexpr qreal MINIMUM = 200.0;
    if (rect.width() < MINIMUM) {
        if (m_canvasHandle == Handle::LEFT || m_canvasHandle == Handle::TOP_LEFT ||
            m_canvasHandle == Handle::BOTTOM_LEFT) {
            rect.setLeft(rect.right() - MINIMUM);
        } else {
            rect.setRight(rect.left() + MINIMUM);
        }
    }
    if (rect.height() < MINIMUM) {
        if (m_canvasHandle == Handle::TOP || m_canvasHandle == Handle::TOP_LEFT ||
            m_canvasHandle == Handle::TOP_RIGHT) {
            rect.setTop(rect.bottom() - MINIMUM);
        } else {
            rect.setBottom(rect.top() + MINIMUM);
        }
    }
    return rect;
}

void CanvasResizeController::UpdateResize(const QPointF& scenePos)
{
    if (!m_resizingCanvas) {
        return;
    }
    m_scene->ApplyCanvasRect(RectForDrag(scenePos));
}

void CanvasResizeController::EndResize()
{
    if (!m_resizingCanvas) {
        return;
    }
    m_resizingCanvas = false;
    m_canvasHandle = Handle::NONE;
    m_scene->CommitCanvasRect(m_resizeStartRect);
    const QSizeF size = m_scene->CanvasSize();
    emit m_view->statusMessage(
        TrView("Canvas size: %1 × %2 (undoable)").arg(qRound(size.width())).arg(qRound(size.height())));
    m_view->RefreshCursor();
}

void CanvasResizeController::UpdateHoverCursor(const QPoint& viewportPos, bool panActive, bool strokeActive)
{
    if (panActive || strokeActive || m_resizingCanvas) {
        return;
    }
    // 编辑模式下左键不再兼作平移，画板边缘始终可以按提示拉伸。
    const Handle handle = HandleAt(viewportPos);
    if (handle == m_hoverHandle) {
        return;
    }
    m_hoverHandle = handle;
    if (handle == Handle::NONE) {
        m_view->RefreshCursor();
    } else {
        m_view->viewport()->setCursor(HandleCursor(handle));
    }
}

void CanvasResizeController::ResetHoverOnLeave()
{
    if (!m_resizingCanvas) {
        m_hoverHandle = Handle::NONE;
        m_view->RefreshCursor();
    }
}

void CanvasResizeController::DrawHandles(QPainter* painter)
{
    const QPolygon polygon = m_view->mapFromScene(m_scene->CanvasRect());
    const QRectF rect = polygon.boundingRect();
    if (rect.width() < 30.0 || rect.height() < 30.0) {
        return;
    }

    const QPointF handles[8] = {rect.topLeft(),     QPointF(rect.center().x(), rect.top()),
                                rect.topRight(),    QPointF(rect.right(), rect.center().y()),
                                rect.bottomRight(), QPointF(rect.center().x(), rect.bottom()),
                                rect.bottomLeft(),  QPointF(rect.left(), rect.center().y())};

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(QColor(59, 130, 246), 1.2));
    painter->setBrush(QColor(255, 255, 255));
    for (const QPointF& point : handles) {
        const QRectF knob(point.x() - 4.5, point.y() - 4.5, 9.0, 9.0);
        painter->drawRoundedRect(knob, 2.0, 2.0);
    }
    painter->restore();
}
