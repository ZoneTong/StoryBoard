#pragma once

#include "IconFactory.h"

class QPainter;

// 图层与视图类图标：可见性、锁定、跨图层移动、层内顺序。
// 返回 true 表示该图标由本单元绘制。
bool PaintIconLayerView(QPainter& painter, IconId id);
