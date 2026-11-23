#pragma once

class CanvasItem;
class PolygonItem;

// D16 白名单的唯一收敛点：多边形内部图层（Layer/Layers）是无法用标量属性表达的
// 叶子数据结构，其它模块统一经 CanvasItemVisitor::VisitPolygon 取指，
// 不再各自 dynamic_cast 到 PolygonItem。
namespace PolygonAccess {

PolygonItem* As(CanvasItem* item);
bool Is(CanvasItem* item);

} // namespace PolygonAccess
