#include "LayerActions.h"

#include "CanvasScene.h"
#include "I18n.h"
#include "LayerPanel.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidget>

LayerActions::LayerActions(CanvasScene* scene, QObject* parent) : QObject(parent), m_scene(scene) {}

void LayerActions::SetLayerVisible(int layerId, bool visible)
{
    if (!m_scene) {
        return;
    }

    const QString layerName =
        (layerId == CanvasScene::BackgroundLayerId) ? LayerPanel::tr("Background layer") : m_scene->layer(layerId).name;
    const int oldActive = m_scene->ActiveLayerId();

    if (!m_scene->SetLayerVisible(layerId, visible)) {
        emit statusMessage(LayerPanel::tr("The last visible layer cannot be hidden. Create a new layer first."));
        emit refreshRequested();
        return;
    }

    if (layerId == CanvasScene::BackgroundLayerId) {
        emit statusMessage(visible ? LayerPanel::tr("Background layer shown")
                                   : LayerPanel::tr("Background layer hidden"));
    } else if (!visible && oldActive == layerId && m_scene->ActiveLayerId() != oldActive) {
        emit statusMessage(LayerPanel::tr("Layer \"%1\" hidden; the current layer switched to \"%2\"")
                               .arg(layerName, m_scene->layer(m_scene->ActiveLayerId()).name));
    } else {
        emit statusMessage(visible ? LayerPanel::tr("Layer \"%1\" shown").arg(layerName)
                                   : LayerPanel::tr("Layer \"%1\" hidden").arg(layerName));
    }
    emit refreshRequested();
}

void LayerActions::SetAllLayersVisible(bool visible)
{
    if (!m_scene) {
        return;
    }
    if (!m_scene->SetAllLayersVisible(visible)) {
        return;
    }
    emit statusMessage(visible ? LayerPanel::tr("All layers shown") : LayerPanel::tr("All layers hidden"));
    emit refreshRequested();
}

void LayerActions::SetLayerLocked(int layerId, bool locked)
{
    if (!m_scene) {
        return;
    }

    const QString layerName = m_scene->layer(layerId).name;
    if (!m_scene->SetLayerLocked(layerId, locked)) {
        emit statusMessage(LayerPanel::tr("The background layer is always locked."));
        emit refreshRequested();
        return;
    }

    emit statusMessage(locked ? LayerPanel::tr("Locked layer \"%1\"; its items can no longer be edited").arg(layerName)
                              : LayerPanel::tr("Unlocked layer \"%1\"").arg(layerName));
    emit refreshRequested();
}

void LayerActions::AddLayer()
{
    if (!m_scene) {
        return;
    }
    m_scene->AddLayerWithUndo();
    emit statusMessage(LayerPanel::tr("Layer created"));
}

void LayerActions::RemoveLayer(QWidget* dialogParent, const QList<int>& layerIds)
{
    if (!m_scene || layerIds.isEmpty()) {
        return;
    }

    for (int id : layerIds) {
        if (m_scene->LayerItemCount(id) > 0) {
            QMessageBox::information(dialogParent, LayerPanel::tr("Delete layer"),
                                     LayerPanel::tr("Layer \"%1\" still contains items. Move or delete them first.")
                                         .arg(m_scene->layer(id).name));
            return;
        }
    }
    if (m_scene->UserLayerCount() - layerIds.size() < 1) {
        QMessageBox::information(dialogParent, LayerPanel::tr("Delete layer"),
                                 LayerPanel::tr("At least one user layer must remain."));
        return;
    }

    for (int id : layerIds) {
        m_scene->RemoveLayerWithUndo(id, nullptr);
    }
    emit statusMessage(layerIds.size() > 1 ? LayerPanel::tr("%1 layer(s) deleted").arg(layerIds.size())
                                           : LayerPanel::tr("Layer deleted"));
    emit refreshRequested();
}

void LayerActions::RenameLayer(QWidget* dialogParent, int layerId)
{
    if (!m_scene || layerId <= CanvasScene::BackgroundLayerId || !m_scene->HasLayer(layerId)) {
        return;
    }

    const CanvasLayer layer = m_scene->layer(layerId);
    bool ok = false;
    const QString name = QInputDialog::getText(dialogParent, LayerPanel::tr("Rename layer"),
                                               LayerPanel::tr("Layer name"), QLineEdit::Normal, layer.name, &ok);
    if (!ok) {
        return;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::information(dialogParent, LayerPanel::tr("Rename layer"),
                                 LayerPanel::tr("Layer name cannot be empty."));
        return;
    }
    if (trimmed == layer.name) {
        return;
    }

    if (m_scene->RenameLayerWithUndo(layerId, trimmed)) {
        emit statusMessage(LayerPanel::tr("Layer renamed to \"%1\"").arg(trimmed));
        emit refreshRequested();
    }
}

void LayerActions::MoveLayer(int delta)
{
    if (!m_scene) {
        return;
    }
    const int id = m_scene->ActiveLayerId();
    if (m_scene->MoveLayerWithUndo(id, delta)) {
        emit statusMessage(delta > 0 ? LayerPanel::tr("Layer moved up") : LayerPanel::tr("Layer moved down"));
    }
}
