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
        // 共用行的可接受集合：内角 A 同时承接三角形底角 A 与平行四边形内角，
        // 箭头行同时承接起止箭头，填充图片行同时承接图片与摆放方式。
        for (PropertyRowSpec& row : rows) {
            if (row.primary == ItemProperty::BOTTOM_ANGLE1) {
                row.accepted.Insert(ItemProperty::PARALLELOGRAM_ANGLE);
            } else if (row.primary == ItemProperty::ARROW_START) {
                row.accepted.Insert(ItemProperty::ARROW_END);
            } else if (row.primary == ItemProperty::ARROW_END) {
                row.accepted.Insert(ItemProperty::ARROW_START);
            } else if (row.primary == ItemProperty::FILL_IMAGE) {
                row.accepted.Insert(ItemProperty::FILL_IMAGE_MODE);
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
