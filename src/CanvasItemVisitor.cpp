#include "CanvasItemVisitor.h"

void CanvasItemVisitor::VisitShape(ShapeItem&) {}

void CanvasItemVisitor::VisitLine(LineItemAbstract&) {}

void CanvasItemVisitor::VisitText(TextItem&) {}
