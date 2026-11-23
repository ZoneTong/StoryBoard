#pragma once

#include "ui/icons/IconFactory.h"

class QPainter;

namespace IconPainting {
// 变换与对齐类图标：旋转、适应窗口、缩放、对齐。
// 返回 true 表示该图标由本单元绘制。
bool PaintTransformAlign(QPainter& painter, IconId id);

} // namespace IconPainting
