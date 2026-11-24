#include "ui/panels/properties/PropertyRowSpec.h"

#include <QHash>

bool PropertyRowSpec::Matches(const ItemPropertySet& properties) const
{
    return !accepted.Intersected(properties).IsEmpty();
}

QString PropertyRowSpec::Label() const
{
    return ItemPropertyCatalog::Label(primary);
}

const QList<PropertyRowSpec>& PropertyRowSpecs::All()
{
    static const QList<PropertyRowSpec> SPECS = []() {
        QList<PropertyRowSpec> rows;
        for (const ItemPropertyDescriptor* descriptor : ItemPropertyCatalog::All()) {
            rows.append(PropertyRowSpec{descriptor->key, ItemPropertySet{descriptor->key}, descriptor});
        }
        // 共用行的可接受集合：成对属性合并到同一行，避免面板只查主属性而漏掉副属性。
        for (PropertyRowSpec& row : rows) {
            if (row.primary == ItemProperty::BOTTOM_ANGLE1) {
                row.accepted.Insert(ItemProperty::PARALLELOGRAM_ANGLE);
                row.accepted.Insert(ItemProperty::BOTTOM_ANGLE2);
            } else if (row.primary == ItemProperty::BOTTOM_ANGLE2) {
                row.accepted.Insert(ItemProperty::BOTTOM_ANGLE1);
            } else if (row.primary == ItemProperty::ARROW_START) {
                row.accepted.Insert(ItemProperty::ARROW_END);
            } else if (row.primary == ItemProperty::ARROW_END) {
                row.accepted.Insert(ItemProperty::ARROW_START);
            } else if (row.primary == ItemProperty::FILL_IMAGE) {
                row.accepted.Insert(ItemProperty::FILL_IMAGE_MODE);
            } else if (row.primary == ItemProperty::MAJOR_AXIS) {
                row.accepted.Insert(ItemProperty::MINOR_AXIS);
            } else if (row.primary == ItemProperty::MINOR_AXIS) {
                row.accepted.Insert(ItemProperty::MAJOR_AXIS);
            } else if (row.primary == ItemProperty::START_ANGLE) {
                row.accepted.Insert(ItemProperty::SWEEP_ANGLE);
            } else if (row.primary == ItemProperty::SWEEP_ANGLE) {
                row.accepted.Insert(ItemProperty::START_ANGLE);
            }
        }
        return rows;
    }();
    return SPECS;
}

const PropertyRowSpec* PropertyRowSpecs::For(ItemProperty property)
{
    return For(PropertyKey(property));
}

const PropertyRowSpec* PropertyRowSpecs::For(PropertyKey property)
{
    if (!property.IsValid()) {
        return nullptr;
    }
    for (const PropertyRowSpec& spec : All()) {
        if (spec.primary == property) {
            return &spec;
        }
    }
    if (property.IsCommon()) {
        return nullptr;
    }
    // 动态参数的行描述按描述符懒建：图元注册什么参数，就有一行可编辑描述，
    // 面板与属性窗都不需要为新增参数追加类型分支。
    static QHash<int, PropertyRowSpec*> dynamicSpecs;
    const auto existing = dynamicSpecs.constFind(property.Id());
    if (existing != dynamicSpecs.constEnd()) {
        return existing.value();
    }
    const ItemPropertyDescriptor* descriptor = ItemPropertyCatalog::Find(property);
    if (!descriptor) {
        return nullptr;
    }
    auto* spec = new PropertyRowSpec{property, ItemPropertySet{property}, descriptor};
    dynamicSpecs.insert(property.Id(), spec);
    return spec;
}
