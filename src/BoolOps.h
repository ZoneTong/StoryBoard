#pragma once

class CanvasScene;

// 多图元布尔运算（合并 / 相减 / 相交），语义参照 PPT 的「合并形状」：
// 结果是一个复合图元，原图元被替换，整个过程只占一条撤销记录；
// 运算本身不可逆，只能撤销 / 重做。
namespace BoolOps {

enum class Operation { Union, Subtract, Intersect };

// 至少两个可编辑图元才能做布尔运算。
bool supports(CanvasScene* scene);
// 返回 true 表示生成了复合图元。
bool apply(CanvasScene* scene, Operation operation, const QString& commandText);

} // namespace BoolOps
