#pragma once

#include "items/shapes/ShapeAbstractItem.h"

#include <QColor>
#include <QIcon>
#include <QList>

// 编辑面板、属性窗与主窗口样式菜单共用的样式名称和缩略图。
// 同一功能在不同入口必须显示同一套文案与图标，避免各画一份后逐渐分叉。
namespace ItemStyleVisuals {

QList<Qt::PenStyle> BorderStyles();
QString BorderStyleName(Qt::PenStyle style);
QIcon BorderStyleIcon(Qt::PenStyle style, int logicalSize = 16);
QIcon FillStyleIcon(FillStyle style, int logicalSize = 16);
QIcon FillStyleIcon(FillStyle style, const QColor& color, int logicalSize);
QIcon FillColorIcon(const QColor& color, int logicalSize = 16);
QIcon BorderColorIcon(const QColor& color, int logicalSize = 16);

} // namespace ItemStyleVisuals
