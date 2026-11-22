#pragma once

#include <QList>
#include <QPainterPath>

namespace MergeRegions {

// 极小碎块阈值（场景坐标绝对平方 px）：所有合并操作与线条分割共用（§5.3，D15）。
constexpr qreal MIN_FRAGMENT_AREA = 16.0;

// 一次合并结果里的一个连续块（场景坐标）。
struct RegionBlock {
    QPainterPath path;    // 场景坐标路径
    qreal area = 0.0;     // PathRegions::Area(path) 派生缓存
    int sourceIndex = -1; // 来源参与者序号；宿主只在同一来源结果内挑选，不跨子图元
    int stackRank = 0;    // 叠放顺序，0 最先画；宿主排序末键（D3）
    bool exempt = false;  // D2：原输入本身 < 阈值，不进入先吸收后删除流程
};

// 先吸收后删除（§5.3）：0 < area < 阈值的块按面积从小到大找合法宿主并入；
// 找不到合法宿主且最终仍 < 阈值才删除。返回保留下来的块，相对顺序保持稳定。
QList<RegionBlock> AbsorbTinyFragments(QList<RegionBlock> blocks);

// 允许区域校验（§5.2）：最终填充落在允许掩膜之外的面积必须为空；不通过不提交，
// 不能事后裁剪或修补。
bool FillWithinMask(const QPainterPath& finalFill, const QPainterPath& allowedMask);

} // namespace MergeRegions
