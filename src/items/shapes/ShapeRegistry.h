#pragma once

#include "items/shapes/ShapeAbstractItem.h"

#include <QList>

// 图形类注册表：ShapeType → 描述器 + 创建器。工厂只查表，不再维护 switch；
// 新增图形只要在 Entries() 里加一行，工厂、持久化字符串与能力标记自动跟上。
class ShapeRegistry {
  public:
    using Creator = ShapeAbstractItem* (*)();

    // 形状描述器：落盘字符串、旧字符串别名、是否有填充面积、是否支持中心点，
    // 以及「常见图形」菜单 / 工具栏顺序（-1 表示不进这份顺序表）。
    struct Descriptor {
        ShapeType type;
        const char* json;
        const char* aliases;
        bool filled;
        bool supportsCenter;
        int menuOrder;
    };

    struct Entry {
        Descriptor descriptor;
        Creator create;
    };

    static const QList<Entry>& Entries();
    // 查表入口：未登记的类型（POLYGON 占位等）返回 nullptr。
    static const Descriptor* Find(ShapeType type);
    // 未知类型（含 POLYGON 占位）退回矩形，避免旧文件缺 shapeType 时图元丢失。
    static ShapeAbstractItem* Create(ShapeType type);
};
