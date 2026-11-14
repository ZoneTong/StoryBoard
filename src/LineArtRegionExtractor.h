#pragma once

#include "LineArtConverter.h"
#include "LineArtRegionModel.h"

#include <QImage>

namespace LineArtInternal {

// 颜色量化取主色 → 8 邻域连通域切块 → 相邻近似色块合并。
// 输入必须是 RGB32 图像；返回逐像素标签与每个色块的统计信息。
RegionMap buildRegions(const QImage& image, const LineArtConverter::Options& options);

} // namespace LineArtInternal
