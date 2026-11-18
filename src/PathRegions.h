#ifndef PATHREGIONS_H
#define PATHREGIONS_H

#include <QList>
#include <QPainterPath>

namespace PathRegions {

// 把一条可能由多块互不相连区域组成的路径拆成「每块一条路径」。
// 布尔运算（尤其剪除）的结果常把底图切成好几块，孔洞仍留在各自那一块里；
// 返回的第一项与原始路径的填充面积一致，只是不再共用一个图元。
QList<QPainterPath> splitDisjoint(const QPainterPath& path);

// 填充面积：孔洞按偶奇规则扣除，供退化碎块（细条、纸屑）识别使用。
qreal area(const QPainterPath& path);

} // namespace PathRegions

#endif // PATHREGIONS_H
