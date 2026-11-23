#pragma once

#include "ui/icons/IconFactory.h"

class QPainter;

namespace IconPainting {
// 形状与图形类图标：形状集合、图片、线稿、背景、网格、调色板、圆角、平滑、不透明度。
// 返回 true 表示该图标由本单元绘制。
bool PaintShapeGraphic(QPainter& painter, IconId id);

} // namespace IconPainting
