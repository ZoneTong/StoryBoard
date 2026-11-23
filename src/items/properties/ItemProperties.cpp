#include "items/properties/ItemProperties.h"

#include <algorithm>

namespace {
constexpr int FIRST_DYNAMIC_KEY_ID = 256;

struct KeyRegistry {
    QHash<QString, int> ids;
    QHash<int, QString> names;
    int nextId = FIRST_DYNAMIC_KEY_ID;
};

KeyRegistry& Registry()
{
    static KeyRegistry registry;
    return registry;
}
} // namespace

PropertyKey PropertyKey::FromStorageKey(const QString& storageKey)
{
    const QString key = storageKey.trimmed();
    if (key.isEmpty()) {
        return PropertyKey();
    }
    KeyRegistry& registry = Registry();
    const auto existing = registry.ids.constFind(key);
    if (existing != registry.ids.constEnd()) {
        return PropertyKey(existing.value());
    }
    const int id = registry.nextId++;
    registry.ids.insert(key, id);
    registry.names.insert(id, key);
    return PropertyKey(id);
}

int PropertyKey::AllocatedCount()
{
    return Registry().ids.size();
}

bool PropertyKey::IsCommon() const
{
    return m_id >= 0 && m_id <= static_cast<int>(ItemProperty::CENTER);
}

ItemProperty PropertyKey::CommonProperty() const
{
    return IsCommon() ? static_cast<ItemProperty>(m_id) : ItemProperty::NAME;
}

QString PropertyKey::StorageKey() const
{
    if (IsCommon()) {
        return QString::number(m_id);
    }
    const auto existing = Registry().names.constFind(m_id);
    return existing != Registry().names.constEnd() ? existing.value() : QString();
}

ItemPropertySet::ItemPropertySet(std::initializer_list<ItemProperty> values)
{
    for (ItemProperty property : values) {
        Insert(property);
    }
}

ItemPropertySet::ItemPropertySet(std::initializer_list<PropertyKey> values)
{
    for (const PropertyKey& property : values) {
        Insert(property);
    }
}

void ItemPropertySet::Insert(ItemProperty property)
{
    m_keys.insert(static_cast<int>(property));
}

void ItemPropertySet::Insert(PropertyKey property)
{
    if (property.IsValid()) {
        m_keys.insert(property.Id());
    }
}

bool ItemPropertySet::Contains(ItemProperty property) const
{
    return m_keys.contains(static_cast<int>(property));
}

bool ItemPropertySet::Contains(PropertyKey property) const
{
    return property.IsValid() && m_keys.contains(property.Id());
}

ItemPropertySet ItemPropertySet::United(const ItemPropertySet& other) const
{
    ItemPropertySet result;
    result.m_keys = m_keys;
    result.m_keys.unite(other.m_keys);
    return result;
}

ItemPropertySet ItemPropertySet::Intersected(const ItemPropertySet& other) const
{
    ItemPropertySet result;
    result.m_keys = m_keys;
    result.m_keys.intersect(other.m_keys);
    return result;
}

QList<ItemProperty> ItemPropertySet::Values() const
{
    QList<ItemProperty> result;
    for (int index = 0; index <= static_cast<int>(ItemProperty::CENTER); ++index) {
        if (m_keys.contains(index)) {
            result.append(static_cast<ItemProperty>(index));
        }
    }
    return result;
}

QList<PropertyKey> ItemPropertySet::Keys() const
{
    QList<int> ids = m_keys.values();
    std::sort(ids.begin(), ids.end());
    QList<PropertyKey> result;
    result.reserve(ids.size());
    for (int id : ids) {
        if (id >= 0) {
            result.append(PropertyKey(id));
        }
    }
    return result;
}
