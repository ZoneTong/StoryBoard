#include "ItemProperties.h"

ItemPropertySet::ItemPropertySet(std::initializer_list<ItemProperty> values)
{
    for (ItemProperty property : values) {
        Insert(property);
    }
}

void ItemPropertySet::Insert(ItemProperty property)
{
    m_bits |= quint64(1) << static_cast<int>(property);
}

bool ItemPropertySet::Contains(ItemProperty property) const
{
    return (m_bits & (quint64(1) << static_cast<int>(property))) != 0;
}

int ItemPropertySet::Size() const
{
    return qPopulationCount(m_bits);
}

ItemPropertySet ItemPropertySet::United(const ItemPropertySet& other) const
{
    ItemPropertySet result;
    result.m_bits = m_bits | other.m_bits;
    return result;
}

ItemPropertySet ItemPropertySet::Intersected(const ItemPropertySet& other) const
{
    ItemPropertySet result;
    result.m_bits = m_bits & other.m_bits;
    return result;
}

QList<ItemProperty> ItemPropertySet::Values() const
{
    QList<ItemProperty> result;
    for (int index = 0; index <= static_cast<int>(ItemProperty::ROTATION); ++index) {
        const ItemProperty property = static_cast<ItemProperty>(index);
        if (Contains(property)) {
            result.append(property);
        }
    }
    return result;
}
