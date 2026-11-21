#include "LineArtConverter.h"

#include "LineArtPipeline.h"
#include "LineArtPreprocessor.h"

#include <QImage>
#include <QtMath>

namespace LineArtConverter {

Result Extract(const QImage& source, const Options& options)
{
    Result result;
    if (source.isNull()) {
        result.error = QStringLiteral("The image is empty.");
        return result;
    }

    // 透明像素按白底处理：线稿化时透明区域不该被切成一堆看不见的色块。
    const QImage base = LineArtInternal::ComposeOnWhite(source);

    const int longEdge = qMax(base.width(), base.height());
    int side = qMin(qMax(64, options.maxProcessingSide), longEdge);
    Result pass = LineArtInternal::ExtractAtSide(base, options, side);
    if (!options.adaptiveDownscale || options.adaptiveRegionLimit <= 0 || !pass.ok) {
        return pass;
    }

    // ---- 自适应降采样：色块太多时缩小处理尺寸重算，而不是把色块直接丢掉 ----
    // 色块数量大致与处理面积的平方成正比，所以按 sqrt(阈值/实测数量) 估缩放比，
    // 每次留 5% 余量，最多重算三轮，并且不低于 minAdaptiveSide。
    const int floorSide = qBound(64, options.minAdaptiveSide, side);
    for (int round = 0; round < 3 && pass.candidateCount > options.adaptiveRegionLimit; ++round) {
        const qreal ratio = qSqrt(static_cast<qreal>(options.adaptiveRegionLimit) / qMax(1, pass.candidateCount));
        const int nextSide = qMax(floorSide, static_cast<int>(side * ratio * 0.95));
        if (nextSide >= side) {
            break;
        }
        side = nextSide;
        pass = LineArtInternal::ExtractAtSide(base, options, side);
        if (!pass.ok) {
            break;
        }
    }
    return pass;
}

} // namespace LineArtConverter
