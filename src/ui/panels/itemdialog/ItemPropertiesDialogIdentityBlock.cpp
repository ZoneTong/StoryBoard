#include "ui/panels/itemdialog/ItemPropertiesDialogIdentityBlock.h"

#include "items/properties/ItemPropertyCatalog.h"
#include "scene/CanvasScene.h"
#include "ui/panels/itemdialog/ItemPropertiesDialog.h"
#include "ui/panels/properties/PropertyRowRegistry.h"
#include "ui/panels/properties/PropertyRowSpec.h"
#include "ui/widgets/rows/ValueRows.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

ItemPropertiesDialogIdentityBlock::ItemPropertiesDialogIdentityBlock(CanvasScene* scene, QWidget* parent)
    : QWidget(parent), m_scene(scene)
{
    // 名称 / 图层 / 高度与编辑面板共用同一批 ValueRows，属性窗不需要行首图标。
    m_nameRow = new TextValueRow(QString(), this);
    m_nameRow->setObjectName(QStringLiteral("ItemPropertiesNameRow"));
    m_nameEdit = m_nameRow->Edit();
    m_nameEdit->setObjectName(QStringLiteral("ItemPropertiesNameValue"));

    m_layerRow = new LayerValueRow(this, false);
    m_layerRow->setObjectName(QStringLiteral("ItemPropertiesLayerRow"));
    m_layerCombo = m_layerRow->Combo();
    m_layerCombo->setObjectName(QStringLiteral("ItemPropertiesLayerValue"));
    m_layerCombo->setMinimumWidth(220);
    connect(m_layerCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_initializing) {
            return;
        }
        UpdateHeightRange(true);
        emit layerChanged();
    });

    m_heightRow = new HeightValueRow(this, false);
    m_heightRow->setObjectName(QStringLiteral("ItemPropertiesHeightRow"));
    m_heightSpin = m_heightRow->Spin();
    m_heightSpin->setObjectName(QStringLiteral("ItemPropertiesHeightValue"));
    m_heightSpin->setMinimumWidth(88);
}

void ItemPropertiesDialogIdentityBlock::AddRows(QFormLayout* form, PropertyRowRegistry* rows)
{
    m_form = form;
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::NAME), m_nameRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::LAYER), m_layerRow);
    form->addRow(ItemPropertyCatalog::Label(ItemProperty::HEIGHT), m_heightRow);
    if (rows) {
        auto registerRow = [rows](ItemProperty property, QWidget* field) {
            if (const PropertyRowSpec* spec = PropertyRowSpecs::For(property)) {
                rows->Register(*spec, field);
            }
        };
        registerRow(ItemProperty::NAME, m_nameRow);
        registerRow(ItemProperty::LAYER, m_layerRow);
        registerRow(ItemProperty::HEIGHT, m_heightRow);
    }
}

void ItemPropertiesDialogIdentityBlock::SetItem(CanvasScene* scene, CanvasItem* item)
{
    m_scene = scene;
    m_item = item;
    // 回填期间屏蔽图层联动，避免赋值触发 UpdateHeightRange / 层内高度重算。
    m_initializing = true;
    if (item) {
        m_nameRow->Refresh(item->Name());
        m_layerRow->Refresh(scene, item->LayerId());
        m_heightSpin->setValue(qMax(0, item->ZOrder()));
    }
}

void ItemPropertiesDialogIdentityBlock::UpdateHeightRange(bool selectTop)
{
    if (!m_layerCombo || !m_heightSpin || !m_item) {
        return;
    }

    const int targetLayer = m_layerCombo->currentData().toInt();
    int maxHeight = 0;
    if (m_scene) {
        for (CanvasItem* peer : m_scene->LayerItems(targetLayer)) {
            maxHeight = qMax(maxHeight, peer->ZOrder());
        }
    }
    // Height 0 is valid and deletion can leave gaps. Allow one slot above the
    // highest existing height, and never clamp the current value on open.
    maxHeight = qMax(0, maxHeight + 1);
    if (!selectTop) {
        maxHeight = qMax(maxHeight, m_item->ZOrder());
    }
    maxHeight = qBound(0, maxHeight, 999999);
    m_heightSpin->setMaximum(maxHeight);
    if (selectTop) {
        m_heightSpin->setValue(maxHeight);
    }
}
