#pragma once

#include "lineart/LineArtConverter.h"

#include <QImage>

namespace LineArtInternal {

// 单轮处理：把已合成白底的图缩到 side 后切块、逐块描边并装配成 Result。
LineArtConverter::Result ExtractAtSide(const QImage& base, const LineArtConverter::Options& options, int side);

} // namespace LineArtInternal
