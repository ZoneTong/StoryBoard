#pragma once

#include "items/properties/ItemProperties.h"

#include <QList>
#include <QString>

// 属性的展示与取值元数据。属性枚举只回答“是什么”，本表回答“叫什么、怎么编辑、
// 允许什么范围”，编辑面板与属性窗共用同一份，避免标签/范围在两侧各写一遍。
enum class ItemPropertyGroup { IDENTITY, TEXT, FILL, STROKE, VALUE, GEOMETRY };

enum class ItemPropertyKind { TEXT, COLOR, ENUM, INTEGER, REAL };

struct ItemPropertyDescriptor {
    ItemProperty property = ItemProperty::NAME;
    const char* key = "";
    const char* label = "";
    ItemPropertyGroup group = ItemPropertyGroup::IDENTITY;
    ItemPropertyKind kind = ItemPropertyKind::TEXT;
    double minimum = 0.0;
    double maximum = 0.0;
    double step = 1.0;
    int decimals = 0;
    const char* suffix = "";
    // 派生属性不落盘，读写只折算到其它存储字段；序列化与属性 key 迁移要跳过。
    bool derived = false;
};

class ItemPropertyCatalog {
  public:
    static const QList<ItemPropertyDescriptor>& All();
    static const ItemPropertyDescriptor* Find(ItemProperty property);
    static QString Label(ItemProperty property);
    static QString Key(ItemProperty property);
};
