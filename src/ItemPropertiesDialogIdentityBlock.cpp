#include "ItemPropertiesDialogIdentityBlock.h"

#include "CanvasScene.h"
#include "ItemPropertiesDialog.h"
#include "PropertyRowWidgets.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

ItemPropertiesDialogIdentityBlock::ItemPropertiesDialogIdentityBlock(CanvasScene* scene, QWidget* parent)
    : QWidget(parent), m_scene(scene)
{
    // 名称 / 图层 / 高度与编辑面板共用同一批 PropertyRowWidgets，属性窗不需要行首图标。
    m_nameRow = new TextAttributeRow(QString(), this);
    m_nameRow->setObjectName(QStringLiteral("ItemPropertiesNameRow"));
    m_nameEdit = m_nameRow->Edit();
    m_nameEdit->setObjectName(QStringLiteral("ItemPropertiesNameValue"));

    m_layerRow = new LayerAttributeRow(this, false);
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

    m_heightRow = new HeightAttributeRow(this, false);
    m_heightRow->setObjectName(QStringLiteral("ItemPropertiesHeightRow"));
    m_heightSpin = m_heightRow->Spin();
    m_heightSpin->setObjectName(QStringLiteral("ItemPropertiesHeightValue"));
    m_heightSpin->setMinimumWidth(88);
}

void ItemPropertiesDialogIdentityBlock::AddRows(QFormLayout* form)
{
    m_form = form;
    form->addRow(ItemPropertiesDialog::tr("Item name"), m_nameRow);
    form->addRow(ItemPropertiesDialog::tr("Layer"), m_layerRow);
    form->addRow(ItemPropertiesDialog::tr("Height"), m_heightRow);
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
    if (m_form) {
        m_form->setRowVisible(m_nameRow, item != nullptr);
        m_form->setRowVisible(m_layerRow, item != nullptr);
        m_form->setRowVisible(m_heightRow, item != nullptr);
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
