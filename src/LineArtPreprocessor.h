#pragma once

#include <QImage>

namespace LineArtInternal {

// 透明像素按白底合成：线稿化时透明区域不该被切成一堆看不见的色块。
QImage composeOnWhite(const QImage& source);

// 长边超过 side 时按比例缩到 side（保持长宽比、平滑变换），统一成 RGB32；否则原样返回。
QImage fitToSide(const QImage& image, int side);

} // namespace LineArtInternal
