#include "PropertyRowRegistry.h"

#include <QFormLayout>
#include <utility>

void PropertyRowRegistry::Register(ItemProperty property, QWidget* field)
{
    Register(ItemPropertySet{property}, field);
}

void PropertyRowRegistry::Register(const ItemPropertySet& accepted, QWidget* field)
{
    if (!field) {
        return;
    }
    m_rows.append(Row{accepted, field});
}

void PropertyRowRegistry::Apply(QFormLayout* form, const ItemPropertySet& properties)
{
    if (!form) {
        return;
    }
    for (const Row& row : std::as_const(m_rows)) {
        form->setRowVisible(row.field, !row.accepted.Intersected(properties).IsEmpty());
    }
}
