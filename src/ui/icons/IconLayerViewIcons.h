#pragma once

#include "ui/icons/IconFactory.h"

class QPainter;

namespace IconPainting {
// 图层与视图类图标：可见性、锁定、跨图层移动、层内顺序。
// 返回 true 表示该图标由本单元绘制。
bool PaintLayerView(QPainter& painter, IconId id);

} // namespace IconPainting
