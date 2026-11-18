#pragma once

#include <QString>

class CanvasScene;

// 需求 12 方案 2：以直线或曲线为刀线分割图元。
// 选中 ≥1 个有填充空间的图元与恰好 1 条线类切刀（直线图元或曲线中心线，
// 直线箭头的箭头装饰忽略），刀线两端沿端点切向延伸到画布边界，把画布分成
// 两个闭合区域；每个区域与被切图元的填充轮廓求交，交集按互不相连拆成多块，
// 每块生成一个多边形图元。碎片样式沿用被切图元；切刀保留在画布上。
// 整个过程只占一条撤销记录；刀线没有切到任何填充图形时提示且不入栈。
namespace LineSplitOps {

// 选中集里必须有填充图元与恰好一条线类切刀。
bool supports(CanvasScene* scene);
// 返回 true 表示至少切出了一块碎片。
bool apply(CanvasScene* scene, const QString& commandText);

} // namespace LineSplitOps
