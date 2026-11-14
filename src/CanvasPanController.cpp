#include "CanvasPanController.h"

#include "CanvasView.h"

#include <QApplication>

CanvasPanController::CanvasPanController(CanvasView* view) : m_view(view) {}

void CanvasPanController::beginPan(const QPoint& position)
{
    m_panning = true;
    m_panStart = position;
    m_panCenterScene = m_view->mapToScene(m_view->viewport()->rect()).boundingRect().center();
    m_panMoved = false;
    m_view->viewport()->setCursor(Qt::ClosedHandCursor);
}

void CanvasPanController::endPan()
{
    m_panning = false;
    m_panMoved = false;
    m_view->updateCursor();
}

bool CanvasPanController::movePan(const QPoint& position)
{
    if (!m_panning) {
        return false;
    }

    if (!m_panMoved && (position - m_panStart).manhattanLength() > QApplication::startDragDistance()) {
        m_panMoved = true;
    }
    // 用“按下点 → 当前点”的场景位移来平移：两个点在同一时刻的变换下相减，
    // 结果与当前视图位置无关。若拿按下时的场景锚点去和当前 mapToScene 相减，
    // 视图每移动一次都会把上次的位移重复计入，画面就会来回抖动。
    const QPointF sceneDelta = m_view->mapToScene(position) - m_view->mapToScene(m_panStart);
    m_view->centerOn(m_panCenterScene - sceneDelta);
    return true;
}
