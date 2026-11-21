#include "CanvasViewCursor.h"

#include "CanvasView.h"

CanvasViewCursor::CanvasViewCursor(CanvasView* view) : m_view(view) {}

void CanvasViewCursor::Refresh(ToolMode mode, bool panning)
{
    if (panning) {
        m_view->viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    switch (mode) {
    case ToolMode::VIEW:
        m_view->viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case ToolMode::BRUSH:
        m_view->viewport()->setCursor(Qt::CrossCursor);
        break;
    case ToolMode::EDIT:
        m_view->viewport()->setCursor(Qt::ArrowCursor);
        break;
    case ToolMode::TEXT:
        m_view->viewport()->setCursor(Qt::IBeamCursor);
        break;
    }
}
