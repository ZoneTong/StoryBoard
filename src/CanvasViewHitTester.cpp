#include "CanvasViewHitTester.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasView.h"

CanvasViewHitTester::CanvasViewHitTester(CanvasView* view, CanvasScene* scene) : m_view(view), m_scene(scene) {}

CanvasItem* CanvasViewHitTester::topItemAt(const QPoint& viewportPos) const
{
    const QList<QGraphicsItem*> candidates = m_scene->items(m_view->mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (canvasItem && canvasItem->isVisible()) {
            return canvasItem;
        }
    }
    return nullptr;
}

CanvasItem* CanvasViewHitTester::propertyItemAt(const QPoint& viewportPos) const
{
    // 只按可见性和真实叠放顺序命中，不检查当前工具模式或图层锁定状态；
    // 属性窗会决定自己是否可编辑。
    const QList<QGraphicsItem*> candidates = m_scene->items(m_view->mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (canvasItem && canvasItem->isVisible()) {
            return canvasItem;
        }
    }
    return nullptr;
}

CanvasItem* CanvasViewHitTester::preferredEditableItemAt(const QPoint& viewportPos) const
{
    const QList<int> preferred = m_scene->preferredLayerIds();
    if (preferred.isEmpty()) {
        return nullptr;
    }

    // 保持顶层优先：列表中越靠前叠得越高。
    const QList<QGraphicsItem*> candidates = m_scene->items(m_view->mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (!canvasItem || !canvasItem->isVisible()) {
            continue;
        }
        if (preferred.contains(canvasItem->LayerId()) && m_scene->isItemEditable(canvasItem)) {
            return canvasItem;
        }
    }
    return nullptr;
}

CanvasItem* CanvasViewHitTester::editableItemAt(const QPoint& viewportPos) const
{
    // 命中要“穿透”锁定或隐藏的图层：自上而下找第一个真正可编辑的图元，
    // 这样锁定的顶层图元不会挡住下面未锁定图层上的图元。
    const QList<QGraphicsItem*> candidates = m_scene->items(m_view->mapToScene(viewportPos));
    const QList<int> preferred = m_scene->preferredLayerIds();
    CanvasItem* fallback = nullptr;
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (!canvasItem || !canvasItem->isVisible()) {
            continue;
        }
        if (!m_scene->isItemEditable(canvasItem)) {
            continue;
        }
        // 重叠区域优先命中图层面板中已选图层里的图元，其余仍按叠放顺序取最上层。
        if (preferred.contains(canvasItem->LayerId())) {
            return canvasItem;
        }
        if (!fallback) {
            fallback = canvasItem;
        }
    }
    return fallback;
}
