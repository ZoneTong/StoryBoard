#pragma once

#include <QString>

class CanvasScene;

// 多图元合并形状，语义与 PPT 的「合并形状」一致，子命令顺序也保持一致：
// 联合 / 异或（XOR，并集减重叠）/ 相交 / 剪除 / 拆分。
// 参与者只有填充图形与文字；直线、直线箭头、曲线、括号、弧与纯描线多边形
// 都属于线条类，不参与合并。除拆分外生成一个多边形；拆分只在填充图形之间
// 按重叠关系切成最多 8 块可分别选中的多边形。原图元被替换，整个过程只占
// 一条撤销记录；运算本身不可逆，只能撤销 / 重做。
namespace BoolOps {

enum class Operation { Union, Xor, Intersect, Subtract, Fragment, LineSplit };

// 至少两个可编辑图元才能做合并形状。
bool supports(CanvasScene* scene);
// 返回 true 表示生成了多边形。
bool apply(CanvasScene* scene, Operation operation, const QString& commandText);

} // namespace BoolOps
