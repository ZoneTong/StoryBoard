#include "ui/panels/properties/PropertiesPanelIdentityCard.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/widgets/rows/ValueRows.h"

#include <QLineEdit>
#include <QVBoxLayout>

PropertiesPanelIdentityCard::PropertiesPanelIdentityCard(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_nameRow = new TextValueRow(PropertiesPanel::tr("Item name"), this);
    m_nameRow->Edit()->setToolTip(PropertiesPanel::tr("Item name"));
    layout->addWidget(m_nameRow);

    m_layerRow = new LayerValueRow(this);
    layout->addWidget(m_layerRow);

    m_heightRow = new HeightValueRow(this);
    layout->addWidget(m_heightRow);

    connect(m_nameRow, &TextValueRow::textChosen, this, [this](const QString& name) {
        if (!m_refreshing && m_item && name != m_item->Name()) {
            emit nameChosen(name);
        }
    });
    connect(m_layerRow, &LayerValueRow::layerChosen, this, [this](int layerId) {
        if (!m_refreshing && m_item && layerId != m_item->LayerId()) {
            emit layerChosen(layerId);
        }
    });
    connect(m_heightRow, &HeightValueRow::heightChosen, this, [this](int height) {
        if (!m_refreshing && m_item && height != m_item->ZOrder()) {
            emit heightChosen(height);
        }
    });
}

void PropertiesPanelIdentityCard::Refresh(CanvasScene* scene, CanvasItem* item)
{
    m_scene = scene;
    m_item = item;
    const bool enabled = scene && item;
    setEnabled(enabled);
    if (!enabled) {
        m_refreshing = true;
        m_nameRow->Refresh(QString());
        m_heightRow->Refresh(0, 0);
        m_refreshing = false;
        return;
    }

    m_refreshing = true;
    m_nameRow->Refresh(item->Name());
    m_layerRow->Refresh(scene, item->LayerId());

    // 与属性窗一致：同层最高高度上再加一格；不改当前值，允许保留下来的空洞。
    int maximum = 0;
    for (CanvasItem* peer : scene->LayerItems(item->LayerId())) {
        maximum = qMax(maximum, peer->ZOrder());
    }
    m_heightRow->Refresh(item->ZOrder(), qMax(maximum + 1, item->ZOrder()));
    m_refreshing = false;
}
