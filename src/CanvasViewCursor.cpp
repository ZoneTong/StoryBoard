#include "CanvasViewCursor.h"

#include "CanvasView.h"

CanvasViewCursor::CanvasViewCursor(CanvasView* view) : m_view(view) {}

void CanvasViewCursor::refresh(ToolMode mode, bool panning)
{
    if (panning) {
        m_view->viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    switch (mode) {
    case ToolMode::View:
        m_view->viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case ToolMode::Brush:
        m_view->viewport()->setCursor(Qt::CrossCursor);
        break;
    case ToolMode::Edit:
        m_view->viewport()->setCursor(Qt::ArrowCursor);
        break;
    case ToolMode::Text:
        m_view->viewport()->setCursor(Qt::IBeamCursor);
        break;
    }
}
