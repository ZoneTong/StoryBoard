#pragma once

#include <QString>

class CanvasScene;

// 多图元合并形状，语义参照 PPT 的「合并形状」五个命令：
// 联合 / 组合（去重叠）/ 拆分 / 相交 / 剪除。
// 除拆分外生成一个合并图形；拆分按块生成多个可分别选中的合并图形。
// 原图元被替换，整个过程只占一条撤销记录；运算本身不可逆，只能撤销 / 重做。
namespace BoolOps {

enum class Operation { Union, Combine, Fragment, Intersect, Subtract };

// 至少两个可编辑图元才能做合并形状。
bool supports(CanvasScene* scene);
// 返回 true 表示生成了合并图形。
bool apply(CanvasScene* scene, Operation operation, const QString& commandText);

} // namespace BoolOps
