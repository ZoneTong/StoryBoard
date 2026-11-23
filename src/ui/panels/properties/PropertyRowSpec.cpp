#include "ui/panels/properties/PropertyRowSpec.h"

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
        for (const ItemPropertyDescriptor& descriptor : ItemPropertyCatalog::All()) {
            rows.append(PropertyRowSpec{descriptor.property, ItemPropertySet{descriptor.property}, &descriptor});
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
    for (const PropertyRowSpec& spec : All()) {
        if (spec.primary == property) {
            return &spec;
        }
    }
    return nullptr;
}
