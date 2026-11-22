#include "CanvasItemVisitor.h"

void CanvasItemVisitor::VisitShape(ShapeAbstractItem&) {}

void CanvasItemVisitor::VisitLine(LineAbstractItem&) {}

void CanvasItemVisitor::VisitCurve(CurveItem&) {}

void CanvasItemVisitor::VisitText(TextItem&) {}
