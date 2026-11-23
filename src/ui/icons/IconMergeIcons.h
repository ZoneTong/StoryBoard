#pragma once

#include "ui/icons/IconFactory.h"

class QPainter;

namespace IconPainting {
// 合并与布尔类图标：联合、组合、拆分、相交、相减。
// 返回 true 表示该图标由本单元绘制。
bool PaintMerge(QPainter& painter, IconId id);

} // namespace IconPainting
