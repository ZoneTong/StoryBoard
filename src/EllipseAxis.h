#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

// 椭圆族的长短半轴推导：外接框即内容矩形，长轴沿当前较长的边。
// 只做纯数值换算，不依赖具体图元类；写回时中心不动，调用方据此改 pos 与内容尺寸。
namespace EllipseAxis {

qreal Major(const QSizeF& size);
qreal Minor(const QSizeF& size);

struct ResizeResult {
    QPointF pos;
    QSizeF size;
};

// majorAxis 为 true 改长半轴，否则改短半轴；两轴保持"长 ≥ 短"的既有钳制关系。
ResizeResult Apply(bool majorAxis, qreal value, const QPointF& pos, const QSizeF& size);

} // namespace EllipseAxis
