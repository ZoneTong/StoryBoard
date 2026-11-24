#pragma once

#include <QPointF>
#include <QRectF>

// 内角旋钮的公共位置口径：可调内角 A 的三种图元（平行四边形 / 菱形 / 三角形）
// 都把旋钮放在“被编辑角的顶点 + 内角平分线”上，离顶点一段屏幕恒定距离。
// 旋钮因此始终贴着它改变的那个角，不会随图元高矮 / 倾斜量跑到框顶去，
// 三种图元的抓取手感与视觉位置保持一致。
namespace VertexAngleKnob {
// 旋钮中心到角顶点的屏幕距离（逻辑像素）：贴住角，又给角上的缩放手柄留出空隙。
qreal GapPixels();

// 顶点 + 平分线方向（两条邻边方向之和，无需归一化）× 间距；形很矮 / 很小时向内收。
QPointF Position(const QPointF& vertex, const QPointF& neighborA, const QPointF& neighborB, qreal viewScale,
                 const QRectF& content);

// 指针绕顶点转过的角度（度，向上为正，0~180 连续）：拖拽按角度增量改 A，
// 三种图元同一套手感；图元有旋转时先换回本地朝向。
qreal PointerAngleDegrees(const QPointF& vertexScene, const QPointF& pointerScene, qreal rotationDegrees);
} // namespace VertexAngleKnob
