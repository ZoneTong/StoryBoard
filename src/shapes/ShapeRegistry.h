#pragma once

#include "ShapeAbstractItem.h"

#include <QList>

// 图形类注册表：ShapeType → 创建器。工厂只查表，不再维护 switch；
// 新增图形只要在 Entries() 里加一行，工厂与自检创建入口自动跟上。
class ShapeRegistry {
  public:
    using Creator = ShapeAbstractItem* (*)();

    struct Entry {
        ShapeType type;
        Creator create;
    };

    static const QList<Entry>& Entries();
    // 未知类型（含 POLYGON 占位）退回矩形，避免旧文件缺 shapeType 时图元丢失。
    static ShapeAbstractItem* Create(ShapeType type);
};
