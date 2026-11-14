#include "CanvasResizeController.h"

#include "CanvasScene.h"
#include "CanvasView.h"

#include <QCoreApplication>
#include <QPainter>
#include <QtMath>

namespace {
QString trView(const char* text)
{
    return QCoreApplication::translate("CanvasView", text);
}
} // namespace

CanvasResizeController::CanvasResizeController(CanvasView* view, CanvasScene* scene) : m_view(view), m_scene(scene) {}

Qt::CursorShape CanvasResizeController::handleCursor(Handle handle)
{
    switch (handle) {
    case Handle::Left:
    case Handle::Right:
        return Qt::SizeHorCursor;
    case Handle::Top:
    case Handle::Bottom:
        return Qt::SizeVerCursor;
    case Handle::TopLeft:
    case Handle::BottomRight:
        return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Handle::None:
        break;
    }
    return Qt::ArrowCursor;
}

CanvasResizeController::Handle CanvasResizeController::handleAt(const QPoint& viewportPos) const
{
    if (m_view->mode() != ToolMode::Edit) {
        return Handle::None;
    }

    const QRectF rect = m_view->mapFromScene(m_scene->canvasRect()).boundingRect();
    const QPointF point = viewportPos;
    const qreal tolerance = 7.0;
    const QRectF outer = rect.adjusted(-tolerance, -tolerance, tolerance, tolerance);
    if (!outer.contains(point)) {
        return Handle::None;
    }

    const bool left = qAbs(point.x() - rect.left()) <= tolerance;
    const bool right = qAbs(point.x() - rect.right()) <= tolerance;
    const bool top = qAbs(point.y() - rect.top()) <= tolerance;
    const bool bottom = qAbs(point.y() - rect.bottom()) <= tolerance;

    if (left && top) {
        return Handle::TopLeft;
    }
    if (right && top) {
        return Handle::TopRight;
    }
    if (left && bottom) {
        return Handle::BottomLeft;
    }
    if (right && bottom) {
        return Handle::BottomRight;
    }
    if (left) {
        return Handle::Left;
    }
    if (right) {
        return Handle::Right;
    }
    if (top) {
        return Handle::Top;
    }
    if (bottom) {
        return Handle::Bottom;
    }
    return Handle::None;
}

bool CanvasResizeController::beginResize(const QPoint& viewportPos)
{
    const Handle handle = handleAt(viewportPos);
    if (handle == Handle::None) {
        return false;
    }

    m_canvasHandle = handle;
    m_resizingCanvas = true;
    m_resizeStartRect = m_scene->canvasRect();
    m_view->viewport()->setCursor(handleCursor(handle));
    return true;
}

QRectF CanvasResizeController::rectForDrag(const QPointF& scenePos) const
{
    QRectF rect = m_resizeStartRect;
    switch (m_canvasHandle) {
    case Handle::Left:
        rect.setLeft(scenePos.x());
        break;
    case Handle::Right:
        rect.setRight(scenePos.x());
        break;
    case Handle::Top:
        rect.setTop(scenePos.y());
        break;
    case Handle::Bottom:
        rect.setBottom(scenePos.y());
        break;
    case Handle::TopLeft:
        rect.setTopLeft(scenePos);
        break;
    case Handle::TopRight:
        rect.setTopRight(scenePos);
        break;
    case Handle::BottomLeft:
        rect.setBottomLeft(scenePos);
        break;
    case Handle::BottomRight:
        rect.setBottomRight(scenePos);
        break;
    case Handle::None:
        return rect;
    }

    rect = rect.normalized();
    constexpr qreal minimum = 200.0;
    if (rect.width() < minimum) {
        if (m_canvasHandle == Handle::Left || m_canvasHandle == Handle::TopLeft ||
            m_canvasHandle == Handle::BottomLeft) {
            rect.setLeft(rect.right() - minimum);
        } else {
            rect.setRight(rect.left() + minimum);
        }
    }
    if (rect.height() < minimum) {
        if (m_canvasHandle == Handle::Top || m_canvasHandle == Handle::TopLeft || m_canvasHandle == Handle::TopRight) {
            rect.setTop(rect.bottom() - minimum);
        } else {
            rect.setBottom(rect.top() + minimum);
        }
    }
    return rect;
}

void CanvasResizeController::updateResize(const QPointF& scenePos)
{
    if (!m_resizingCanvas) {
        return;
    }
    m_scene->applyCanvasRect(rectForDrag(scenePos));
}

void CanvasResizeController::endResize()
{
    if (!m_resizingCanvas) {
        return;
    }
    m_resizingCanvas = false;
    m_canvasHandle = Handle::None;
    m_scene->commitCanvasRect(m_resizeStartRect);
    const QSizeF size = m_scene->canvasSize();
    emit m_view->statusMessage(
        trView("Canvas size: %1 × %2 (undoable)").arg(qRound(size.width())).arg(qRound(size.height())));
    m_view->updateCursor();
}

void CanvasResizeController::updateHoverCursor(const QPoint& viewportPos, bool panActive, bool strokeActive)
{
    if (panActive || strokeActive || m_resizingCanvas) {
        return;
    }
    // 编辑模式下左键不再兼作平移，画板边缘始终可以按提示拉伸。
    const Handle handle = handleAt(viewportPos);
    if (handle == m_hoverHandle) {
        return;
    }
    m_hoverHandle = handle;
    if (handle == Handle::None) {
        m_view->updateCursor();
    } else {
        m_view->viewport()->setCursor(handleCursor(handle));
    }
}

void CanvasResizeController::resetHoverOnLeave()
{
    if (!m_resizingCanvas) {
        m_hoverHandle = Handle::None;
        m_view->updateCursor();
    }
}

void CanvasResizeController::drawHandles(QPainter* painter)
{
    const QPolygon polygon = m_view->mapFromScene(m_scene->canvasRect());
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
