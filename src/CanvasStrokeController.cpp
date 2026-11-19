#include "CanvasStrokeController.h"

#include "CanvasScene.h"
#include "CanvasView.h"
#include "CurveItem.h"
#include "LineItem.h"
#include "ShapeItem.h"

#include <QCoreApplication>
#include <QLineF>
#include <QVector>
#include <QtMath>

namespace {
QString trView(const char* text)
{
    return QCoreApplication::translate("CanvasView", text);
}

// 需求 8：笔画拟合出的节点几乎共线时视作直线。阈值放宽到约笔画宽度的一半，
// 手绘轻微抖动的直线不会被误判成曲线；折返笔画（来回描）按曲线保留。
bool strokeLooksLinear(const CurveItem* stroke)
{
    const QVector<QPointF> knots = stroke->strokePoints();
    if (knots.size() < 2) {
        return false;
    }
    const QLineF axis(knots.first(), knots.last());
    const qreal length = axis.length();
    if (length < 2.0) {
        return false;
    }
    const qreal tolerance = qMax(1.5, stroke->strokeWidth() * 0.5);
    qreal drawn = 0.0;
    for (const QPointF& knot : knots) {
        if (axis.dx() == 0.0 && axis.dy() == 0.0) {
            return false;
        }
        // 点到首尾连线的距离：叉积除以底边长。
        const qreal cross = axis.dx() * (knot.y() - knots.first().y()) - axis.dy() * (knot.x() - knots.first().x());
        if (qAbs(cross) / length > tolerance) {
            return false;
        }
    }
    for (int i = 1; i < knots.size(); ++i) {
        drawn += QLineF(knots.at(i - 1), knots.at(i)).length();
    }
    // 累计弧长明显超过首尾距（≥1.15 倍）说明笔画有折返或绕行，不算直线。
    return drawn <= length * 1.15;
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

void CanvasStrokeController::setStrokeSmoothing(qreal tolerance)
{
    m_strokeSmoothing = qBound<qreal>(0.0, tolerance, 20.0);
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

    // 需求 8：画笔模式画出的近似直线的笔画直接落成直线类型图元。
    // 需求 20：笔画收尾后节点已被平移到图元局部坐标，必须经 mapToScene
    // 换回场景坐标再落位，否则直线会跑到画布原点附近"消失"。
    if (stroke->source() == CurveSource::HandDrawn && strokeLooksLinear(stroke)) {
        const QVector<QPointF> knots = stroke->strokePoints();
        const QPointF a = stroke->mapToScene(knots.first());
        const QPointF b = stroke->mapToScene(knots.last());
        // 直线是曲线族的两点线性子型：端点直接决定几何，不再用旋转角摆放细长条。
        auto* line = new LineItem();
        line->setEndpoints(a, b, stroke->strokeColor(), stroke->strokeWidth());
        line->setStrokeStyle(stroke->strokeStyle());
        m_scene->removeItem(stroke);
        delete stroke;
        m_scene->commitNewItem(line, trView("Add stroke"));
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
