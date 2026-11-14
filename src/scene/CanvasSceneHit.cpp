#include "CanvasItem.h"
#include "CanvasScene.h"
#include "scene/ObscurationAnalyzer.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRegion>
#include <QStyleOptionGraphicsItem>
#include <QVector>
#include <algorithm>

QList<CanvasItem*> CanvasScene::obscuredItems(const QRectF& area) const
{
    // 查看模式下要求完全遮盖（什么提示都不画）；导出同样不画提示轮廓。
    if (m_mode == ToolMode::View || m_exporting) {
        return {};
    }
    return ObscurationAnalyzer().findObscured(canvasItems(), area);
}

void CanvasScene::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsScene::drawForeground(painter, rect);

    // 非查看模式下，被高层级/更高 height 图元遮住的图元补画一圈虚线轮廓，
    // 让用户仍能看出那里有东西（查看模式与导出不画）。
    const QList<CanvasItem*> obscured = obscuredItems(rect);
    if (!painter || obscured.isEmpty()) {
        return;
    }

    painter->save();
    painter->setClipRect(rect);
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 两层描边：先铺一圈半透明白色柔光，再压深色虚线。无论被遮住的是浅色
    // 画板还是深色图元，虚线轮廓都能看清。
    QPen haloPen(QColor(255, 255, 255, 120));
    haloPen.setWidthF(2.6);
    haloPen.setCosmetic(true);
    haloPen.setStyle(Qt::SolidLine);

    QPen dashPen(QColor(30, 41, 59, 205));
    dashPen.setWidthF(1.2);
    dashPen.setCosmetic(true);
    dashPen.setStyle(Qt::DashLine);
    dashPen.setDashPattern({5.0, 4.0});

    for (CanvasItem* item : obscured) {
        const QPainterPath path = item->sceneTransform().map(item->outlineShape());
        if (path.isEmpty()) {
            continue;
        }
        painter->setPen(haloPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
        painter->setPen(dashPen);
        painter->drawPath(path);
    }
    painter->restore();
}

void CanvasScene::drawItems(QPainter* painter, int numItems, QGraphicsItem* items[],
                            const QStyleOptionGraphicsItem* options, QWidget* widget)
{
    if (numItems <= 1) {
        QGraphicsScene::drawItems(painter, numItems, items, options, widget);
        return;
    }

    // 遮挡必须是“完全不透”：被更高优先级图元涂满的地方，下层图元的颜色整个不画。
    // 直接按画家算法叠半透明颜色会混出第三种颜色，所以这里给每个图元算一块可见
    // 区域——从它上方所有图元涂满的区域里减掉，只画剩下的部分。
    // QGraphicsView 用 exposedRegion 过滤图元，画笔上通常没有设置裁剪；
    // 这种情况下 clipBoundingRect() 返回的是空矩形（不是整个设备），
    // 所以为空时自己取设备范围换算回场景坐标，否则会误判成“全部被遮住”。
    QRectF clipBounds = painter->clipBoundingRect();
    if (clipBounds.isEmpty()) {
        if (QPaintDevice* device = painter->device()) {
            clipBounds = painter->worldTransform().inverted().mapRect(QRectF(0, 0, device->width(), device->height()));
        }
    }
    const QRegion visibleBase(clipBounds.toAlignedRect());

    QList<int> order;
    order.reserve(numItems);
    for (int i = 0; i < numItems; ++i) {
        order.append(i);
    }
    std::stable_sort(order.begin(), order.end(),
                     [items](int a, int b) { return items[a]->zValue() < items[b]->zValue(); });

    QList<QRectF> bounds;
    bounds.reserve(order.size());
    for (int index : order) {
        bounds.append(items[index]->sceneBoundingRect());
    }

    // 上方图元的涂满区域按需计算：只有包围盒真的相交时才会用到。
    QList<QRegion> regions;
    regions.resize(order.size());
    QVector<bool> regionReady(order.size(), false);
    auto regionOf = [&](int position) -> const QRegion& {
        if (!regionReady.at(position)) {
            if (auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(items[order.at(position)])) {
                regions[position] =
                    ObscurationAnalyzer::filledRegion(canvasItem->sceneTransform().map(canvasItem->paintedShape()));
            } else {
                regions[position] = QRegion(bounds.at(position).toAlignedRect());
            }
            regionReady[position] = true;
        }
        return regions.at(position);
    };

    for (int position = 0; position < order.size(); ++position) {
        QRegion covered;
        for (int above = position + 1; above < order.size(); ++above) {
            if (!bounds.at(above).intersects(bounds.at(position))) {
                continue;
            }
            covered = covered.united(regionOf(above));
        }

        const QRegion visible = visibleBase.subtracted(covered);
        if (visible.isEmpty()) {
            continue;
        }

        const int index = order.at(position);
        painter->save();
        painter->setClipRegion(visible, Qt::IntersectClip);
        QGraphicsScene::drawItems(painter, 1, &items[index], &options[index], widget);
        painter->restore();
    }
}
