#pragma once

#include "IconFactory.h"

class QPainter;

// 变换与对齐类图标：旋转、适应窗口、缩放、对齐。
// 返回 true 表示该图标由本单元绘制。
bool paintIconTransformAlign(QPainter& painter, IconId id);
