#include "CanvasStrokeController.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "CurveItem.h"

#include <QCoreApplication>

namespace {
QString trView(const char* text)
{
    return QCoreApplication::translate("CanvasView", text);
}
} // namespace

CanvasStrokeController::CanvasStrokeController(CanvasView* view, CanvasScene* scene) : m_view(view), m_scene(scene) {}

void CanvasStrokeController::setBrushColor(const QColor& color)
{
    if (color.isValid()) {
        m_brushColor = color;
    }
}

void CanvasStrokeController::setBrushWidth(qreal width)
{
    m_brushWidth = qBound<qreal>(0.5, width, 64.0);
}

void CanvasStrokeController::setStrokeSmoothing(int tolerance)
{
    m_strokeSmoothing = qBound(0, tolerance, 30);
}

bool CanvasStrokeController::beginStroke(const QPoint& viewportPos)
{
    if (!m_scene->isActiveLayerWritable()) {
        emit m_view->statusMessage(
            trView("The current layer is locked or hidden; drawing is unavailable. Unlock or switch "
                   "layers in the layer panel first."));
        return false;
    }

    auto* stroke = new CurveItem();
    // Keep the in-progress stroke on top while drawing. The final layer
    // and height are assigned when the stroke is committed.
    stroke->setZValue(1.0e12);
    stroke->beginStroke(m_view->mapToScene(viewportPos), m_brushColor, m_brushWidth);
    m_scene->addItem(stroke);
    m_activeStroke = stroke;
    m_scene->clearSelection();
    return true;
}

void CanvasStrokeController::appendPoint(const QPoint& viewportPos)
{
    if (!m_activeStroke) {
        return;
    }
    m_activeStroke->appendScenePoint(m_view->mapToScene(viewportPos));
    m_view->viewport()->update();
}

void CanvasStrokeController::finishStroke()
{
    if (!m_activeStroke) {
        return;
    }

    CurveItem* stroke = m_activeStroke;
    m_activeStroke = nullptr;
    stroke->endStroke(m_strokeSmoothing);

    if (stroke->isEmpty()) {
        m_scene->removeItem(stroke);
        delete stroke;
        m_view->updateCursor();
        return;
    }

    m_scene->commitNewItem(stroke, trView("Add stroke"));
    m_view->updateCursor();
}

void CanvasStrokeController::cancelStroke()
{
    if (!m_activeStroke) {
        return;
    }

    CurveItem* stroke = m_activeStroke;
    m_activeStroke = nullptr;
    m_scene->removeItem(stroke);
    delete stroke;
    m_view->updateCursor();
}
