#ifndef PATHREGIONS_H
#define PATHREGIONS_H

#include <QList>
#include <QPainterPath>

namespace PathRegions {

// 把一条可能由多块互不相连区域组成的路径拆成「每块一条路径」。
// 布尔运算（尤其剪除）的结果常把底图切成好几块，孔洞仍留在各自那一块里；
// 返回的各块并集必须全覆盖原填充区域：拆分不得丢面积；仅角点接触的两块
// 不算连续，保持各自独立成块（§5.1-6、§5.2）。
QList<QPainterPath> SplitDisjoint(const QPainterPath& path);

// 填充面积：孔洞按偶奇规则扣除，供退化碎块（细条、纸屑）识别使用。
qreal Area(const QPainterPath& path);

// 连续判定：共享边或实际重叠（面积 > 0）算连续；仅角点接触不算连续（§5.1-6）。
bool RegionsContinuous(const QPainterPath& a, const QPainterPath& b);

// 两区域共享边界的近似长度（接触链轮廓长度），碎块宿主排序第一键（D3）。
qreal SharedBoundaryLength(const QPainterPath& a, const QPainterPath& b);

// 两区域外接框中心的欧氏距离，碎块宿主排序第二键（D3）；空路径取无穷大。
qreal RegionDistance(const QPainterPath& a, const QPainterPath& b);

} // namespace PathRegions

#endif // PATHREGIONS_H
