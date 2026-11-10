#include "LineArtConverter.h"

#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPair>
#include <QSet>
#include <QtMath>

#include <algorithm>

namespace {

// 8 邻域，从“右”开始顺时针排列。
const int kDirX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
const int kDirY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

int directionIndex(int dx, int dy)
{
    for (int i = 0; i < 8; ++i) {
        if (kDirX[i] == dx && kDirY[i] == dy)
            return i;
    }
    return -1;
}

qreal pointToSegmentDistance(const QPointF& point, const QPointF& start, const QPointF& end)
{
    const QPointF segment = end - start;
    const qreal lengthSquared = QPointF::dotProduct(segment, segment);
    if (lengthSquared <= 0.0)
        return QLineF(point, start).length();

    qreal t = QPointF::dotProduct(point - start, segment) / lengthSquared;
    t = qBound<qreal>(0.0, t, 1.0);
    return QLineF(point, start + segment * t).length();
}

// 迭代版 Ramer-Douglas-Peucker：像素级的阶梯轮廓会被压成少量折线段，
// 用显式栈而不是递归，避免长轮廓把调用栈压爆。
QVector<QPointF> simplifyPolyline(const QVector<QPointF>& points, qreal tolerance)
{
    if (points.size() <= 2 || tolerance <= 0.0)
        return points;

    QVector<bool> keep(points.size(), false);
    keep.first() = true;
    keep.last() = true;

    QVector<QPair<int, int>> pending;
    pending.append(qMakePair(0, points.size() - 1));
    while (!pending.isEmpty()) {
        const QPair<int, int> span = pending.takeLast();
        const int first = span.first;
        const int last = span.second;
        if (last <= first + 1)
            continue;

        qreal maxDistance = -1.0;
        int maxIndex = -1;
        for (int i = first + 1; i < last; ++i) {
            const qreal distance = pointToSegmentDistance(points.at(i), points.at(first),
                                                          points.at(last));
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }
        if (maxDistance > tolerance && maxIndex > first) {
            keep[maxIndex] = true;
            pending.append(qMakePair(first, maxIndex));
            pending.append(qMakePair(maxIndex, last));
        }
    }

    QVector<QPointF> result;
    result.reserve(points.size());
    for (int i = 0; i < points.size(); ++i) {
        if (keep.at(i))
            result.append(points.at(i));
    }
    return result;
}

// Moore 邻域边界跟踪：从一个色块的最左上像素出发绕它走一圈。
// 停止条件用“位置 + 进入方向”这种状态重复来判断（Jacob 停止条件的等价形式）。
// 返回像素中心坐标，首点只出现一次（闭合点由调用方补上）。
QVector<QPointF> traceContour(const QVector<int>& labels, int width, int height, int seed,
                              int regionId)
{
    QVector<QPointF> contour;
    const int startX = seed % width;
    const int startY = seed / width;

    const auto isRegion = [&labels, width, height, regionId](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height
            && labels.at(y * width + x) == regionId;
    };

    int cx = startX;
    int cy = startY;
    // 起点是这一块最左上的像素，它左边一定不是本块（也可能是图像外）。
    int backX = startX - 1;
    int backY = startY;
    QSet<qint64> visitedStates;
    const int guard = width * height + 64;
    for (int step = 0; step < guard; ++step) {
        // backtrack 相对当前像素的方位，扫描从这里开始顺时针绕一圈。
        int backDir = directionIndex(backX - cx, backY - cy);
        if (backDir < 0)
            backDir = 4;

        // 同一个像素、又是从同一方向进来的：这一圈已经走完了。
        const qint64 state = (static_cast<qint64>(cy) * width + cx) * 8 + backDir;
        if (visitedStates.contains(state))
            break;
        visitedStates.insert(state);

        int nextDir = -1;
        for (int i = 0; i < 8; ++i) {
            const int d = (backDir + i) % 8;
            const int nx = cx + kDirX[d];
            const int ny = cy + kDirY[d];
            if (isRegion(nx, ny)) {
                nextDir = d;
                break;
            }
            // 这一格不是本色块，它就是下一轮扫描的 backtrack。
            backX = nx;
            backY = ny;
        }
        if (nextDir < 0)
            break;

        contour.append(QPointF(cx, cy));
        cx += kDirX[nextDir];
        cy += kDirY[nextDir];
    }
    return contour;
}

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

} // namespace

namespace LineArtConverter {

Result extract(const QImage& source, const Options& options)
{
    Result result;
    if (source.isNull()) {
        result.error = QStringLiteral("The image is empty.");
        return result;
    }

    // 透明像素按白底处理：线稿化时透明区域不该被切成一堆看不见的色块。
    QImage image(source.size(), QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.drawImage(0, 0, source);
    }

    // 处理尺寸有上限：超大图先等比缩小，否则连通域和轮廓跟踪都会明显变慢。
    const int maxSide = qMax(64, options.maxProcessingSide);
    if (qMax(image.width(), image.height()) > maxSide) {
        image = image.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_RGB32);
    }

    const int width = image.width();
    const int height = image.height();
    if (width < 4 || height < 4) {
        result.error = QStringLiteral("The image is too small to trace.");
        return result;
    }
    result.imageSize = QSize(width, height);
    const int total = width * height;

    // ---- 颜色量化：统计每个颜色格子，取出现最多的若干种颜色当主色 ----
    const int levels = qBound(2, options.colorLevels, 40);
    const int quantStep = qMax(1, 256 / levels);
    const int bins = 256 / quantStep + 1;
    const int binTotal = bins * bins * bins;

    QVector<qint64> binCount(binTotal, 0);
    QVector<qint64> binR(binTotal, 0);
    QVector<qint64> binG(binTotal, 0);
    QVector<qint64> binB(binTotal, 0);
    QVector<int> binOfPixel(total, 0);

    for (int y = 0; y < height; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const QRgb rgb = line[x];
            const int redBin = qRed(rgb) / quantStep;
            const int greenBin = qGreen(rgb) / quantStep;
            const int blueBin = qBlue(rgb) / quantStep;
            const int key = (redBin * bins + greenBin) * bins + blueBin;
            binOfPixel[y * width + x] = key;
            ++binCount[key];
            binR[key] += qRed(rgb);
            binG[key] += qGreen(rgb);
            binB[key] += qBlue(rgb);
        }
    }

    // ---- 主色聚类：按像素数从多到少扫颜色格子 ----
    // 颜色和已有主色足够接近就并进那一类，否则新建一个主色；累计覆盖率够了就停，
    // 剩下的格子都并到最近的主色。这样颜色种类少时只留真正的几块颜色，
    // 抗锯齿边缘那些过渡色不会被当成独立主色（否则会沿着每个色块描出一圈细环）。
    QVector<int> order;
    order.reserve(binTotal);
    for (int key = 0; key < binTotal; ++key) {
        if (binCount.at(key) > 0)
            order.append(key);
    }
    std::stable_sort(order.begin(), order.end(), [&binCount](int first, int second) {
        return binCount.at(first) > binCount.at(second);
    });

    const int maxColors = qBound(1, options.maxPaletteColors, binTotal);
    const qint64 coverageTarget = static_cast<qint64>(total) * 995 / 1000;
    const qreal mergeDistanceSquared = options.paletteMergeDistance * options.paletteMergeDistance;

    QVector<int> paletteOfBin(binTotal, -1);
    QVector<QColor> palette;
    qint64 covered = 0;
    for (int index = 0; index < order.size(); ++index) {
        const int key = order.at(index);
        const qint64 count = binCount.at(key);
        const QColor color(static_cast<int>(binR.at(key) / count),
                           static_cast<int>(binG.at(key) / count),
                           static_cast<int>(binB.at(key) / count));

        int nearest = -1;
        qint64 nearestDistance = -1;
        for (int i = 0; i < palette.size(); ++i) {
            const qint64 dr = color.red() - palette.at(i).red();
            const qint64 dg = color.green() - palette.at(i).green();
            const qint64 db = color.blue() - palette.at(i).blue();
            const qint64 distance = dr * dr + dg * dg + db * db;
            if (nearestDistance < 0 || distance < nearestDistance) {
                nearestDistance = distance;
                nearest = i;
            }
        }

        const bool mayGrow = palette.size() < maxColors && covered < coverageTarget;
        if (nearest >= 0 && (!mayGrow || nearestDistance <= mergeDistanceSquared)) {
            paletteOfBin[key] = nearest;
        } else {
            palette.append(color);
            paletteOfBin[key] = palette.size() - 1;
        }
        covered += count;
    }

    QVector<int> paletteIndex(total, 0);
    for (int index = 0; index < total; ++index)
        paletteIndex[index] = paletteOfBin.at(binOfPixel.at(index));
    binOfPixel.clear();
    binOfPixel.squeeze();

    // ---- 8 邻域连通域：同一主色相连的像素算一个色块 ----
    QVector<int> labels(total, -1);
    QVector<RegionData> regions;
    QVector<int> pending;
    pending.reserve(1024);

    for (int seed = 0; seed < total; ++seed) {
        if (labels.at(seed) >= 0)
            continue;

        const int palette = paletteIndex.at(seed);
        const int regionId = regions.size();
        RegionData region;
        region.seed = seed;
        region.minX = region.maxX = seed % width;
        region.minY = region.maxY = seed / width;

        labels[seed] = regionId;
        pending.append(seed);
        while (!pending.isEmpty()) {
            const int index = pending.takeLast();
            const int x = index % width;
            const int y = index / width;
            const QRgb rgb = reinterpret_cast<const QRgb*>(image.constScanLine(y))[x];

            ++region.area;
            region.sumR += qRed(rgb);
            region.sumG += qGreen(rgb);
            region.sumB += qBlue(rgb);
            if (x < region.minX)
                region.minX = x;
            if (x > region.maxX)
                region.maxX = x;
            if (y < region.minY)
                region.minY = y;
            if (y > region.maxY)
                region.maxY = y;

            for (int k = 0; k < 8; ++k) {
                const int nx = x + kDirX[k];
                const int ny = y + kDirY[k];
                if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    continue;
                const int neighbour = ny * width + nx;
                if (labels.at(neighbour) >= 0 || paletteIndex.at(neighbour) != palette)
                    continue;
                labels[neighbour] = regionId;
                pending.append(neighbour);
            }
        }
        regions.append(region);
    }
    pending.clear();
    pending.squeeze();

    // ---- 相邻近似色块合并 ----
    // 同一个物件常被纹理、抗锯齿、明暗渐变切成几块颜色接近的碎块。把“空间上相邻、
    // 平均色又足够接近”的碎块并成一个色块再描外轮廓，比直接放宽面积下限更划算：
    // 小道具该描的还在，碎线却被并掉了。
    if (options.adjacentMergeDistance > 0.0 && regions.size() > 1) {
        const int regionCount = regions.size();

        // 相邻关系只记一份：每个像素只看右、右下、下、左下四个方向。
        // 只认 8 邻域贴边：允许跨间隙合并会把中间那条过渡线一起并进来，
        // 整片结构会跟着糊掉（实测 recall 会从 0.96 掉到 0.65）。
        QVector<int> edgeFirst;
        QVector<int> edgeSecond;
        QSet<qint64> seenEdges;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int index = y * width + x;
                const int label = labels.at(index);
                for (int k = 0; k < 4; ++k) {
                    const int nx = x + kDirX[k];
                    const int ny = y + kDirY[k];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;
                    const int other = labels.at(ny * width + nx);
                    if (other == label)
                        continue;
                    const int first = qMin(label, other);
                    const int second = qMax(label, other);
                    const qint64 key = static_cast<qint64>(first) * regionCount + second;
                    if (seenEdges.contains(key))
                        continue;
                    seenEdges.insert(key);
                    edgeFirst.append(first);
                    edgeSecond.append(second);
                }
            }
        }

        QVector<int> parent(regionCount);
        for (int i = 0; i < regionCount; ++i)
            parent[i] = i;
        auto findRoot = [&parent](int value) {
            while (parent.at(value) != value) {
                parent[value] = parent.at(parent.at(value));
                value = parent.at(value);
            }
            return value;
        };

        // 相邻对按“两边平均色差”从小到大逐个吃：只并真正接近的一对，不让合并后的
        // 综合色继续把邻居拉进来（那会把本来有边界的两块颜色滚成一片，结构就糊了）。
        struct AdjacentPair {
            qreal distance;
            int first;
            int second;
        };
        QVector<AdjacentPair> pairs;
        pairs.reserve(edgeFirst.size());
        for (int i = 0; i < edgeFirst.size(); ++i) {
            const RegionData& one = regions.at(edgeFirst.at(i));
            const RegionData& other = regions.at(edgeSecond.at(i));
            const qint64 areaOne = qMax<qint64>(1, one.area);
            const qint64 areaOther = qMax<qint64>(1, other.area);
            const qint64 dr = one.sumR / areaOne - other.sumR / areaOther;
            const qint64 dg = one.sumG / areaOne - other.sumG / areaOther;
            const qint64 db = one.sumB / areaOne - other.sumB / areaOther;
            AdjacentPair pair;
            pair.distance = qSqrt(static_cast<qreal>(dr * dr + dg * dg + db * db));
            pair.first = edgeFirst.at(i);
            pair.second = edgeSecond.at(i);
            pairs.append(pair);
        }

        // 代表色按合并后的面积加权统计，最终笔画颜色才不会偏。
        QVector<qint64> rootArea(regionCount, 0);
        QVector<qint64> rootR(regionCount, 0);
        QVector<qint64> rootG(regionCount, 0);
        QVector<qint64> rootB(regionCount, 0);
        for (int i = 0; i < regionCount; ++i) {
            rootArea[i] = regions.at(i).area;
            rootR[i] = regions.at(i).sumR;
            rootG[i] = regions.at(i).sumG;
            rootB[i] = regions.at(i).sumB;
        }

        std::stable_sort(pairs.begin(), pairs.end(),
                         [](const AdjacentPair& left, const AdjacentPair& right) {
                             return left.distance < right.distance;
                         });
        for (int i = 0; i < pairs.size(); ++i) {
            const AdjacentPair& pair = pairs.at(i);
            if (pair.distance > options.adjacentMergeDistance)
                break;
            int first = findRoot(pair.first);
            int second = findRoot(pair.second);
            if (first == second)
                continue;
            if (rootArea.at(first) < rootArea.at(second))
                qSwap(first, second);
            parent[second] = first;
            rootArea[first] += rootArea.at(second);
            rootR[first] += rootR.at(second);
            rootG[first] += rootG.at(second);
            rootB[first] += rootB.at(second);
        }

        // 合并后重算每个色块的外框、种子像素和颜色统计，并重新编号。
        QVector<int> rootToNew(regionCount, -1);
        QVector<RegionData> mergedRegions;
        QVector<int> mergedLabels(total, -1);
        for (int index = 0; index < total; ++index) {
            const int root = findRoot(labels.at(index));
            int id = rootToNew.at(root);
            if (id < 0) {
                id = mergedRegions.size();
                rootToNew[root] = id;
                RegionData region;
                region.seed = index;
                region.minX = region.maxX = index % width;
                region.minY = region.maxY = index / width;
                mergedRegions.append(region);
            }

            RegionData& region = mergedRegions[id];
            const int x = index % width;
            const int y = index / width;
            const QRgb rgb = reinterpret_cast<const QRgb*>(image.constScanLine(y))[x];
            ++region.area;
            region.sumR += qRed(rgb);
            region.sumG += qGreen(rgb);
            region.sumB += qBlue(rgb);
            if (x < region.minX)
                region.minX = x;
            if (x > region.maxX)
                region.maxX = x;
            if (y < region.minY)
                region.minY = y;
            if (y > region.maxY)
                region.maxY = y;
            mergedLabels[index] = id;
        }

        labels = mergedLabels;
        regions = mergedRegions;
    }

    result.regionCount = regions.size();

    // ---- 挑出色块：去掉整张图的底色和太小的碎块 ----
    const qint64 imageArea = static_cast<qint64>(width) * height;
    QVector<int> candidates;
    candidates.reserve(regions.size());
    for (int index = 0; index < regions.size(); ++index) {
        const RegionData& region = regions.at(index);
        const bool touchesBorder = region.minX <= 0 || region.minY <= 0
            || region.maxX >= width - 1 || region.maxY >= height - 1;
        // 贴着画框又占了很大面积的那一块就是整张图的底色，描它的外框没有意义。
        if (touchesBorder && region.area >= imageArea / 5)
            continue;
        // 横跨整幅（或纵贯整幅）的大块通常是渐变底色被量化出来的条带，同理跳过。
        const bool spansWidth = region.minX <= 0 && region.maxX >= width - 1;
        const bool spansHeight = region.minY <= 0 && region.maxY >= height - 1;
        if ((spansWidth || spansHeight) && region.area >= imageArea / 12)
            continue;
        if (region.area < qMax(4, options.minRegionArea))
            continue;
        candidates.append(index);
    }
    // 大色块先描、小色块后描：小细节压在共享边界上面，看起来更清楚。
    std::stable_sort(candidates.begin(), candidates.end(), [&regions](int first, int second) {
        return regions.at(first).area > regions.at(second).area;
    });
    if (candidates.size() > qMax(1, options.maxRegions)) {
        candidates.resize(qMax(1, options.maxRegions));
        result.truncated = true;
    }
    candidates.squeeze();

    for (int index : candidates) {
        const RegionData& region = regions.at(index);
        const QVector<QPointF> contour = traceContour(labels, width, height, region.seed, index);
        if (contour.size() < 3)
            continue;

        // 轮廓周长 ≈ 走一圈的像素步数；2*面积/周长 就是这一块的平均宽度。
        // 平均宽度太小的条带基本是抗锯齿过渡带，描出来只是一圈没意义的细环。
        const qreal thickness = 2.0 * region.area / qMax(1, contour.size());
        if (options.minRegionThickness > 0.0 && thickness < options.minRegionThickness)
            continue;

        QVector<QPointF> points = simplifyPolyline(contour, options.simplifyTolerance);
        if (points.size() < 2)
            continue;
        // 补上闭合点，让笔画图元自己形成一个完整的回路。
        if (points.constFirst() != points.constLast())
            points.append(points.constFirst());
        if (points.size() < 4)
            continue;

        Outline outline;
        outline.points = points;
        outline.area = region.area;
        const qint64 count = qMax<qint64>(1, region.area);
        outline.color = QColor(static_cast<int>(region.sumR / count),
                               static_cast<int>(region.sumG / count),
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

} // namespace LineArtConverter
