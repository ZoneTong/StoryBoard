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

qreal CanvasZoomController::Zoom() const
{
    return m_view->transform().m11();
}

void CanvasZoomController::FitAll()
{
    if (m_view->viewport()->width() <= 8 || m_view->viewport()->height() <= 8) {
        return;
    }

    QRectF target = m_scene->ContentRect();
    if (target.isEmpty()) {
        target = m_scene->CanvasRect();
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

    EmitZoom();
}

void CanvasZoomController::ZoomIn()
{
    ApplyScale(1.22);
}

void CanvasZoomController::ZoomOut()
{
    ApplyScale(1.0 / 1.22);
}

void CanvasZoomController::ActualSize()
{
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
    m_view->resetTransform();
    m_view->centerOn(center);
    EmitZoom();
}

void CanvasZoomController::ApplyScale(qreal factor)
{
    if (factor <= 0.0) {
        return;
    }

    const qreal current = Zoom();
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
    EmitZoom();
}

void CanvasZoomController::EmitZoom()
{
    if (m_scene) {
        m_scene->RefreshViewDependentGeometry();
    }
    emit m_view->zoomChanged(Zoom());
}

void CanvasZoomController::HandleWheel(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        m_view->QGraphicsView::wheelEvent(event);
        return;
    }

    const qreal current = Zoom();
    qreal factor = qPow(1.0015, delta);
    const qreal target = qBound(MIN_ZOOM, current * factor, MAX_ZOOM);
    factor = target / current;

    if (qFuzzyCompare(factor, 1.0)) {
        event->accept();
        return;
    }

    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->scale(factor, factor);
    EmitZoom();
    event->accept();
}
