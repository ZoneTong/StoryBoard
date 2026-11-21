#pragma once

#include "IconFactory.h"

class QPainter;

// 合并与布尔类图标：联合、组合、拆分、相交、相减。
// 返回 true 表示该图标由本单元绘制。
bool PaintIconMerge(QPainter& painter, IconId id);
