#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// 图形类工厂：按 ShapeType 建出具体子类；未知类型退回矩形，避免旧文件缺 shapeType
// 时图元丢失。
ShapeAbstractItem* CreateShapeItem(ShapeType type);
