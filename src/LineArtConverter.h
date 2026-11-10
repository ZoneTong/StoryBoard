#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVector>

// 把图片按量化后的色块切开，再描出每个色块的轮廓，供“导入图片线稿化”生成笔画图元。
// 处理流程：限制尺寸 → 颜色量化取主色 → 8 邻域连通域切块 → 逐块边界跟踪 → RDP 简化。
namespace LineArtConverter {

struct Options {
    // 处理时图像长边上限。太小会丢细节，太大会变慢。
    int maxProcessingSide = 900;
    // 每个通道的量化级数（2~64）。级数越少，越容易把相近颜色并成同一块。
    int colorLevels = 6;
    // 保留的主色数量。超出这个数量的零碎颜色会归到最接近的主色。
    int maxPaletteColors = 24;
    // 颜色距离小于该值（RGB 欧氏距离）的颜色格子会并进同一个主色，
    // 用来吃掉抗锯齿和细微色差，避免同一个色块被切成一圈圈的过渡带。
    // 调小能让颜色接近底色的小道具独立成块，代价是过渡带可能重新出现。
    qreal paletteMergeDistance = 48.0;
    // 小于该像素面积的色块不描边（按处理尺度统计）。
    int minRegionArea = 12;
    // 相邻色块合并距离：两个相邻色块的平均颜色距离小于该值时，视为同一块颜色的碎块，
    // 合并成一个色块后只描一条外轮廓。0 表示不合并。
    // 它和 paletteMergeDistance 的区别：后者合并的是“同一种颜色”，会把挨在一起的
    // 相近颜色直接并进同一个主色；前者发生在连通域之后，只动空间上相邻的碎块，
    // 因此可以放心把 paletteMergeDistance 调小来保住小道具，再用这里消除碎线。
    qreal adjacentMergeDistance = 48.0;
    // 最多描多少个色块；超出的色块直接丢弃，并在 Result::truncated 里报告。
    int maxRegions = 1200;
    // 轮廓简化容差（处理尺度像素）。线稿按折线绘制，容差留小一点，圆弧才不像多边形；
    // 太小的话像素阶梯又会被当成真实拐点留下来。
    qreal simplifyTolerance = 0.6;
    // 抗锯齿产生的极细条带（平均宽度小于该像素值）不描边，避免沿着色块描出一圈细环。
    qreal minRegionThickness = 1.0;
    // 生成笔画的线宽（场景坐标）。按需求固定为 1，不能再粗。
    qreal strokeWidth = 1.0;
};

struct Outline {
    // 处理尺度下的像素坐标；闭合轮廓的首尾点相同。
    QVector<QPointF> points;
    // 该色块的平均颜色，用作笔画颜色。
    QColor color;
    int area = 0;
};

struct Result {
    bool ok = false;
    // ok 为 false 时的原因（英文源串，交给调用方翻译）。
    QString error;
    // 处理尺度下的图像尺寸，调用方据此换算到场景坐标。
    QSize imageSize;
    QVector<Outline> outlines;
    // 识别到的色块数量（含被当作背景跳过的整片底色）。
    int regionCount = 0;
    // 色块太多，只描了前 maxRegions 个。
    bool truncated = false;
};

Result extract(const QImage& image, const Options& options = Options());

} // namespace LineArtConverter
