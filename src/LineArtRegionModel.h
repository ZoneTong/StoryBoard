#pragma once

#include <QVector>

namespace LineArtInternal {

// 单个色块在处理尺度下的统计信息：面积、种子像素、外框与颜色分量累加值。
struct RegionData {
    int area = 0;
    int seed = -1;
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    qint64 sumR = 0;
    qint64 sumG = 0;
    qint64 sumB = 0;
};

// 一轮切块的结果：逐像素标签（-1 表示未归类）与每个色块的统计。
struct RegionMap {
    int width = 0;
    int height = 0;
    QVector<int> labels;
    QVector<RegionData> regions;
};

} // namespace LineArtInternal
