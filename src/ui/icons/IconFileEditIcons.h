#pragma once

#include "ui/icons/IconFactory.h"

class QPainter;

// 文件与编辑类图标：新建、打开、保存、导出、撤销/重做、画笔、编辑、文字、删除、文件夹、设置。
// 返回 true 表示该图标由本单元绘制。
bool PaintIconFileEdit(QPainter& painter, IconId id);
