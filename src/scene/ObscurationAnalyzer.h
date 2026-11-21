#pragma once

#include <QList>
#include <QRectF>
#include <QRegion>

class QPainterPath;
class CanvasItem;

// 遮挡（命中）相关的纯几何计算：按图层/高度优先级判断哪些图元被更高优先级图元盖住，
// 并提供把图元涂满轮廓展开成像素区域的工具。不持有场景状态，供场景绘制与提示轮廓复用。
class ObscurationAnalyzer {
  public:
    // 返回被任一更高优先级图元盖住的可见图元；area 非空时只统计与该区域相交的图元。
    QList<CanvasItem*> FindObscured(const QList<CanvasItem*>& items, const QRectF& area) const;

    // 用路径填充多边形近似成区域，便于快速算出被遮盖的面积占比。
    static QRegion FilledRegion(const QPainterPath& path);
    static qreal RegionArea(const QRegion& region);
};
