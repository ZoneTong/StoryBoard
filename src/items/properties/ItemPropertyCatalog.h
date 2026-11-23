#pragma once

#include "items/properties/ItemProperties.h"

#include <QList>
#include <QString>
#include <functional>

class CanvasItem;

// 属性的展示与取值元数据。属性枚举只回答“是什么”，本表回答“叫什么、怎么编辑、
// 允许什么范围”，编辑面板与属性窗共用同一份，避免标签/范围在两侧各写一遍。
enum class ItemPropertyGroup { IDENTITY, TEXT, FILL, STROKE, VALUE, GEOMETRY };

enum class ItemPropertyKind { TEXT, COLOR, ENUM, INTEGER, REAL };

struct ItemPropertyDescriptor {
    PropertyKey key;
    QString storageKey;
    QString label;
    ItemPropertyGroup group = ItemPropertyGroup::IDENTITY;
    ItemPropertyKind kind = ItemPropertyKind::TEXT;
    double minimum = 0.0;
    double maximum = 0.0;
    double step = 1.0;
    int decimals = 0;
    QString suffix;
    // 派生属性不落盘，读写只折算到其它存储字段；序列化与属性 key 迁移要跳过。
    bool derived = false;
    // 描述符可自带读写回调；逐步把 switch (property) 的分派收进配置表。
    std::function<QVariant(const CanvasItem*)> reader;
    std::function<void(CanvasItem*, const QVariant&)> writer;
};

class ItemPropertyCatalog {
  public:
    static const QList<ItemPropertyDescriptor*>& All();
    static const ItemPropertyDescriptor* Find(ItemProperty property);
    static const ItemPropertyDescriptor* Find(PropertyKey property);
    // 图元专属参数：按稳定字符串注册一条描述，测试与后续叶子图形共用同一入口。
    static const ItemPropertyDescriptor* RegisterDynamic(PropertyKey property, const QString& label,
                                                         ItemPropertyGroup group, ItemPropertyKind kind,
                                                         double minimum = 0.0, double maximum = 0.0, double step = 1.0,
                                                         int decimals = 0, const QString& suffix = QString(),
                                                         std::function<QVariant(const CanvasItem*)> reader = {},
                                                         std::function<void(CanvasItem*, const QVariant&)> writer = {});
    static QString Label(ItemProperty property);
    static QString Label(PropertyKey property);
    static QString Key(ItemProperty property);
    static QString Key(PropertyKey property);
};
