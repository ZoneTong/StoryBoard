#pragma once

#include "items/properties/ItemPropertyCatalog.h"

#include <QList>

// 一行属性在 UI 里的最小描述：主属性、可接受属性集合与编辑器类型。
// 编辑面板与属性窗共用同一份行描述，显隐只看图元上报的属性集合是否与 accepted 相交。
struct PropertyRowSpec {
    PropertyKey primary;
    ItemPropertySet accepted;
    const ItemPropertyDescriptor* descriptor = nullptr;

    bool Matches(const ItemPropertySet& properties) const;
    QString Label() const;
};

class PropertyRowSpecs {
  public:
    static const QList<PropertyRowSpec>& All();
    static const PropertyRowSpec* For(ItemProperty property);
    static const PropertyRowSpec* For(PropertyKey property);
};
