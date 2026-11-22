#include "ops/MergeRegions.h"

#include "ops/PathRegions.h"

#include <limits>

namespace {

constexpr qreal AREA_EPS = 0.05;
constexpr qreal BOUNDARY_EPS = 1.0;
constexpr qreal DISTANCE_EPS = 1.0;

// 宿主候选记录：排序键按 D3 依次为共享边界长度↓ → 距离↑ → 宿主面积↓ → 叠放顺序↓。
struct HostPick {
    int index = -1;
    qreal boundary = 0.0;
    qreal distance = std::numeric_limits<qreal>::max();
    qreal area = 0.0;
    int rank = std::numeric_limits<int>::max();
};

bool HostOutranks(const HostPick& current, const HostPick& candidate)
{
    if (current.index < 0) {
        return true;
    }
    if (candidate.boundary > current.boundary + BOUNDARY_EPS) {
        return true;
    }
    if (qAbs(candidate.boundary - current.boundary) <= BOUNDARY_EPS) {
        if (candidate.distance < current.distance - DISTANCE_EPS) {
            return true;
        }
        if (qAbs(candidate.distance - current.distance) <= DISTANCE_EPS) {
            if (candidate.area > current.area + AREA_EPS) {
                return true;
            }
            if (qAbs(candidate.area - current.area) <= AREA_EPS && candidate.rank < current.rank) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

namespace MergeRegions {

QList<RegionBlock> AbsorbTinyFragments(QList<RegionBlock> blocks)
{
    QList<RegionBlock> result = blocks;
    bool changed = true;
    while (changed) {
        changed = false;
        // 挑当前面积最小的极小碎块（豁免块不进入先吸收后删除流程）。
        int fragmentIndex = -1;
        for (int i = 0; i < result.size(); ++i) {
            const RegionBlock& block = result.at(i);
            if (block.exempt || block.area <= 0.0 || block.area >= MIN_FRAGMENT_AREA) {
                continue;
            }
            if (fragmentIndex < 0 || block.area < result.at(fragmentIndex).area) {
                fragmentIndex = i;
            }
        }
        if (fragmentIndex < 0) {
            break;
        }
        const RegionBlock fragment = result.at(fragmentIndex);

        HostPick host;
        for (int i = 0; i < result.size(); ++i) {
            if (i == fragmentIndex) {
                continue;
            }
            const RegionBlock& candidate = result.at(i);
            // 宿主面积不得小于被吸收碎块；宿主自身可 < 阈值，但不能比碎块小。
            if (candidate.area < fragment.area - AREA_EPS) {
                continue;
            }
            // 同一来源结果内挑选，分组内不跨子图元。
            if (fragment.sourceIndex >= 0 && candidate.sourceIndex != fragment.sourceIndex) {
                continue;
            }
            if (!PathRegions::RegionsContinuous(candidate.path, fragment.path)) {
                continue;
            }
            const QPainterPath united = candidate.path.united(fragment.path);
            // 并集仍必须是一块连续填充。
            if (PathRegions::SplitDisjoint(united).size() != 1) {
                continue;
            }
            // 不新增像素：并集面积不得超过两者面积之和（不能跨缝补洞）。
            if (PathRegions::Area(united) > candidate.area + fragment.area + AREA_EPS) {
                continue;
            }
            HostPick pick;
            pick.index = i;
            pick.boundary = PathRegions::SharedBoundaryLength(candidate.path, fragment.path);
            pick.distance = PathRegions::RegionDistance(candidate.path, fragment.path);
            pick.area = candidate.area;
            pick.rank = candidate.stackRank;
            if (HostOutranks(host, pick)) {
                host = pick;
            }
        }

        if (host.index >= 0) {
            // 吸收：宿主路径并入碎块，重算连续性与面积；填充/描边按宿主层保留。
            RegionBlock& target = result[host.index];
            target.path = target.path.united(fragment.path);
            target.area = PathRegions::Area(target.path);
            result.removeAt(fragmentIndex);
        } else {
            // 找不到合法宿主，且当前块仍 < 阈值：删除。
            result.removeAt(fragmentIndex);
        }
        changed = true;
    }
    return result;
}

bool FillWithinMask(const QPainterPath& finalFill, const QPainterPath& allowedMask)
{
    if (finalFill.isEmpty()) {
        return true;
    }
    if (allowedMask.isEmpty()) {
        return PathRegions::Area(finalFill) <= AREA_EPS;
    }
    return PathRegions::Area(finalFill.subtracted(allowedMask)) <= AREA_EPS;
}

} // namespace MergeRegions
