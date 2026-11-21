#include "LineArtPipeline.h"

#include "LineArtContourProcessor.h"
#include "LineArtPreprocessor.h"
#include "LineArtRegionExtractor.h"
#include "LineArtRegionModel.h"

#include <QColor>
#include <QSize>
#include <QVector>
#include <algorithm>

namespace LineArtInternal {

using LineArtConverter::Options;
using LineArtConverter::Outline;
using LineArtConverter::Result;

Result ExtractAtSide(const QImage& base, const Options& options, int side)
{
    Result result;
    const QImage image = FitToSide(base, side);

    const int width = image.width();
    const int height = image.height();
    if (width < 4 || height < 4) {
        result.error = QStringLiteral("The image is too small to trace.");
        return result;
    }
    result.imageSize = QSize(width, height);

    const RegionMap regions = BuildRegions(image, options);
    result.regionCount = regions.regions.size();

    // ---- 挑出色块：去掉整张图的底色和太小的碎块 ----
    const qint64 imageArea = static_cast<qint64>(width) * height;
    QVector<int> candidates;
    candidates.reserve(regions.regions.size());
    for (int index = 0; index < regions.regions.size(); ++index) {
        const RegionData& region = regions.regions.at(index);
        const bool touchesBorder =
            region.minX <= 0 || region.minY <= 0 || region.maxX >= width - 1 || region.maxY >= height - 1;
        // 贴着画框又占了很大面积的那一块就是整张图的底色，描它的外框没有意义。
        if (touchesBorder && region.area >= imageArea / 5) {
            continue;
        }
        // 横跨整幅（或纵贯整幅）的大块通常是渐变底色被量化出来的条带，同理跳过。
        const bool spansWidth = region.minX <= 0 && region.maxX >= width - 1;
        const bool spansHeight = region.minY <= 0 && region.maxY >= height - 1;
        if ((spansWidth || spansHeight) && region.area >= imageArea / 12) {
            continue;
        }
        if (region.area < qMax(4, options.minRegionArea)) {
            continue;
        }
        candidates.append(index);
    }
    // 大色块先描、小色块后描：小细节压在共享边界上面，看起来更清楚。
    std::stable_sort(candidates.begin(), candidates.end(), [&regions](int first, int second) {
        return regions.regions.at(first).area > regions.regions.at(second).area;
    });
    if (options.maxRegions > 0 && candidates.size() > options.maxRegions) {
        candidates.resize(options.maxRegions);
        result.truncated = true;
    }
    // 候选色块数量（截断与描边之前的实测值），自适应降采样据此判断要不要重算。
    result.candidateCount = candidates.size();
    candidates.squeeze();

    for (int index : candidates) {
        const RegionData& region = regions.regions.at(index);
        const QVector<QPointF> contour = TraceContour(regions.labels, width, height, region.seed, index);
        const QVector<QPointF> points = BuildOutlinePoints(contour, region.area, options);
        if (points.isEmpty()) {
            continue;
        }

        Outline outline;
        outline.points = points;
        outline.area = region.area;
        const qint64 count = qMax<qint64>(1, region.area);
        outline.color = QColor(static_cast<int>(region.sumR / count), static_cast<int>(region.sumG / count),
                               static_cast<int>(region.sumB / count));
        result.outlines.append(outline);
    }

    if (result.outlines.isEmpty()) {
        result.error = QStringLiteral("No color blocks were found in the image.");
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace LineArtInternal
