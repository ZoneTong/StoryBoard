#include "scene/LayerDomain.h"

#include <QtGlobal>

void LayerDomain::Reset()
{
    m_layers.clear();
    m_activeLayerId = 1;
    m_nextLayerId = 2;
}

void LayerDomain::SetLayers(const QList<CanvasLayer>& layers)
{
    m_layers = layers;
    for (const CanvasLayer& entry : m_layers) {
        ObserveId(entry.id);
    }
}

bool LayerDomain::HasLayer(int layerId) const
{
    return Index(layerId) >= 0;
}

CanvasLayer LayerDomain::Layer(int layerId) const
{
    for (const CanvasLayer& entry : m_layers) {
        if (entry.id == layerId) {
            return entry;
        }
    }
    return {};
}

int LayerDomain::Index(int layerId) const
{
    for (int index = 0; index < m_layers.size(); ++index) {
        if (m_layers.at(index).id == layerId) {
            return index;
        }
    }
    return -1;
}

QList<CanvasLayer> LayerDomain::UserLayers() const
{
    if (m_layers.size() <= 1) {
        return {};
    }
    return m_layers.mid(1);
}

int LayerDomain::NextId()
{
    return m_nextLayerId++;
}

void LayerDomain::ObserveId(int layerId)
{
    if (layerId > 0) {
        m_nextLayerId = qMax(m_nextLayerId, layerId + 1);
    }
}
