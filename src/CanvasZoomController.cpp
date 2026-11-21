#include "CanvasZoomController.h"

#include "CanvasScene.h"
#include "CanvasView.h"

#include <QTransform>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr qreal MIN_ZOOM = 0.02;
constexpr qreal MAX_ZOOM = 40.0;
const qreal FIT_MAX_ZOOM = 3.0;
} // namespace

CanvasZoomController::CanvasZoomController(CanvasView* view, CanvasScene* scene) : m_view(view), m_scene(scene) {}

qreal CanvasZoomController::zoom() const
{
    return m_view->transform().m11();
}

void CanvasZoomController::fitAll()
{
    if (m_view->viewport()->width() <= 8 || m_view->viewport()->height() <= 8) {
        return;
    }

    QRectF target = m_scene->contentRect();
    if (target.isEmpty()) {
        target = m_scene->canvasRect();
    }

    const qreal margin = qMax<qreal>(24.0, qMax(target.width(), target.height()) * 0.035);
    target = target.adjusted(-margin, -margin, margin, margin);

    m_view->fitInView(target, Qt::KeepAspectRatio);

    const qreal fitted = m_view->transform().m11();
    const bool tooLarge = fitted > FIT_MAX_ZOOM;
    const bool tooSmall = fitted < MIN_ZOOM;
    if (tooLarge || tooSmall) {
        const qreal clamped = qBound(MIN_ZOOM, fitted, FIT_MAX_ZOOM);
        m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
        m_view->setTransform(QTransform::fromScale(clamped, clamped));
        m_view->centerOn(target.center());
    }

    emitZoom();
}

void CanvasZoomController::zoomIn()
{
    applyScale(1.22);
}

void CanvasZoomController::zoomOut()
{
    applyScale(1.0 / 1.22);
}

void CanvasZoomController::actualSize()
{
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
    m_view->resetTransform();
    m_view->centerOn(center);
    emitZoom();
}

void CanvasZoomController::applyScale(qreal factor)
{
    if (factor <= 0.0) {
        return;
    }

    const qreal current = zoom();
    if (current <= 0.0) {
        return;
    }

    const qreal target = qBound(MIN_ZOOM, current * factor, MAX_ZOOM);
    const qreal applied = target / current;
    if (qFuzzyCompare(applied, 1.0)) {
        return;
    }

    m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_view->scale(applied, applied);
    emitZoom();
}

void CanvasZoomController::emitZoom()
{
    if (m_scene) {
        m_scene->refreshViewDependentGeometry();
    }
    emit m_view->zoomChanged(zoom());
}

void CanvasZoomController::handleWheel(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        m_view->QGraphicsView::wheelEvent(event);
        return;
    }

    const qreal current = zoom();
    qreal factor = qPow(1.0015, delta);
    const qreal target = qBound(MIN_ZOOM, current * factor, MAX_ZOOM);
    factor = target / current;

    if (qFuzzyCompare(factor, 1.0)) {
        event->accept();
        return;
    }

    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->scale(factor, factor);
    emitZoom();
    event->accept();
}
