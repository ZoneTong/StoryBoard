#include "scene/ObscurationAnalyzer.h"

#include "items/CanvasItem.h"

#include <QPainterPath>
#include <QPolygon>
#include <QVector>
#include <algorithm>

QRegion ObscurationAnalyzer::FilledRegion(const QPainterPath& path)
{
    QRegion region;
    if (path.isEmpty()) {
        return region;
    }
    for (const QPolygonF& polygon : path.toFillPolygons()) {
        const QPolygon points = polygon.toPolygon();
        if (points.size() >= 3) {
            region = region.united(QRegion(points));
        }
    }
    return region;
}

qreal ObscurationAnalyzer::RegionArea(const QRegion& region)
{
    qreal area = 0.0;
    for (const QRect& rect : region) {
        area += qreal(rect.width()) * qreal(rect.height());
    }
    return area;
}

QList<CanvasItem*> ObscurationAnalyzer::FindObscured(const QList<CanvasItem*>& items, const QRectF& area) const
{
    QList<CanvasItem*> stack = items;
    stack.erase(std::remove_if(stack.begin(), stack.end(),
                               [&area](CanvasItem* item) {
                                   if (!item || !item->isVisible()) {
                                       return true;
                                   }
                                   return !area.isEmpty() && !item->sceneBoundingRect().intersects(area);
                               }),
                stack.end());
    std::stable_sort(stack.begin(), stack.end(),
                     [](const CanvasItem* a, const CanvasItem* b) { return a->zValue() < b->zValue(); });

    const int count = stack.size();
    QList<QRectF> bounds;
    QList<QRegion> regions;
    QVector<bool> regionReady;
    bounds.reserve(count);
    regions.reserve(count);
    regionReady.reserve(count);
    for (int i = 0; i < count; ++i) {
        bounds.append(stack.at(i)->sceneBoundingRect());
        regions.append(QRegion());
        regionReady.append(false);
    }

    // PaintedShape() 会为笔画、图形做描边展开，只在真正需要比较时才计算。
    auto regionOf = [&stack, &regions, &regionReady](int index) -> const QRegion& {
        if (!regionReady.at(index)) {
            CanvasItem* item = stack.at(index);
            regions[index] = FilledRegion(item->sceneTransform().map(item->PaintedShape()));
            regionReady[index] = true;
        }
        return regions.at(index);
    };

    QList<CanvasItem*> result;
    for (int i = 0; i < count; ++i) {
        CanvasItem* lower = stack.at(i);
        const QRegion own = regionOf(i);
        const qreal ownArea = RegionArea(own);
        if (ownArea <= 0.0) {
            continue;
        }

        QRegion covered;
        for (int j = i + 1; j < count; ++j) {
            CanvasItem* upper = stack.at(j);
            // layer 更大，或同层 height 更大，就是更高优先级。
            if (upper->zValue() <= lower->zValue()) {
                continue;
            }
            if (!bounds.at(j).intersects(bounds.at(i))) {
                continue;
            }
            covered = covered.united(own.intersected(regionOf(j)));
        }

        // 只要有任何面积被更高优先级图元盖住，就算被遮挡：被盖住的内容完全看不见，
        // 但会给它补一圈轮廓。
        if (RegionArea(covered) > 0.0) {
            result.append(lower);
        }
    }
    return result;
}
