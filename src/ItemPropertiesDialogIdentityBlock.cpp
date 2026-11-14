#include "ItemPropertiesDialogIdentityBlock.h"

#include "CanvasScene.h"
#include "ItemPropertiesDialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

ItemPropertiesDialogIdentityBlock::ItemPropertiesDialogIdentityBlock(CanvasScene* scene, CanvasItem* item,
                                                                     QWidget* parent)
    : QWidget(parent), m_scene(scene), m_item(item)
{
    if (!item) {
        return;
    }

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("ItemPropertiesNameValue"));
    m_nameEdit->setText(item->name());
    m_nameEdit->setPlaceholderText(item->typeDisplayName());

    m_layerCombo = new QComboBox(this);
    m_layerCombo->setObjectName(QStringLiteral("ItemPropertiesLayerValue"));
    m_layerCombo->setMinimumWidth(220);
    if (scene) {
        for (const CanvasLayer& layer : scene->userLayers()) {
            m_layerCombo->addItem(ItemPropertiesDialog::layerDescription(scene, layer.id), layer.id);
        }
    }
    const int layerIndex = m_layerCombo->findData(item->layerId());
    if (layerIndex >= 0) {
        m_layerCombo->setCurrentIndex(layerIndex);
    }
    connect(m_layerCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_initializing) {
            return;
        }
        updateHeightRange(true);
        emit layerChanged();
    });

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setObjectName(QStringLiteral("ItemPropertiesHeightValue"));
    m_heightSpin->setRange(0, 999999);
    m_heightSpin->setKeyboardTracking(false);
    m_heightSpin->setValue(qMax(0, item->height()));
}

void ItemPropertiesDialogIdentityBlock::addRows(QFormLayout* form)
{
    if (m_nameEdit) {
        form->addRow(ItemPropertiesDialog::tr("Item name"), m_nameEdit);
    }
    if (m_layerCombo) {
        form->addRow(ItemPropertiesDialog::tr("Layer"), m_layerCombo);
    }
    if (m_heightSpin) {
        form->addRow(ItemPropertiesDialog::tr("Height"), m_heightSpin);
    }
}

void ItemPropertiesDialogIdentityBlock::updateHeightRange(bool selectTop)
{
    if (!m_layerCombo || !m_heightSpin || !m_item) {
        return;
    }

    const int targetLayer = m_layerCombo->currentData().toInt();
    int maxHeight = 0;
    if (m_scene) {
        for (CanvasItem* peer : m_scene->layerItems(targetLayer)) {
            maxHeight = qMax(maxHeight, peer->height());
        }
    }
    // Height 0 is valid and deletion can leave gaps. Allow one slot above the
    // highest existing height, and never clamp the current value on open.
    maxHeight = qMax(0, maxHeight + 1);
    if (!selectTop) {
        maxHeight = qMax(maxHeight, m_item->height());
    }
    maxHeight = qBound(0, maxHeight, 999999);
    m_heightSpin->setMaximum(maxHeight);
    if (selectTop) {
        m_heightSpin->setValue(maxHeight);
    }
}
