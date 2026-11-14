#include "CanvasScene.h"

#include "CanvasItem.h"
#include "scene/SceneConstants.h"

#include <QUndoStack>

CanvasScene::CanvasScene(QObject* parent) : QGraphicsScene(parent)
{
    m_layers = {{BackgroundLayerId, scene_constants::kBackgroundLayerName, true, true},
                {scene_constants::kFirstUserLayerId, QStringLiteral("图层 1"), true, false}};
    m_activeLayerId = scene_constants::kFirstUserLayerId;
    m_nextLayerId = scene_constants::kFirstUserLayerId + 1;

    setBackgroundBrush(Qt::NoBrush);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    setStickyFocus(true);
    updateSceneExtent();
}

CanvasScene::~CanvasScene()
{
    clearOrphans();
}

void CanvasScene::setUndoStack(QUndoStack* stack)
{
    m_undoStack = stack;
}

void CanvasScene::setMode(ToolMode mode)
{
    if (m_mode == mode) {
        return;
    }

    if (m_mode == ToolMode::Edit) {
        clearSelection();
    }

    m_mode = mode;
    updateItemFlags();
    update();
    emit modeChanged(mode);
}

void CanvasScene::resetDocument()
{
    if (m_undoStack) {
        m_undoStack->clear();
    }

    clear();
    clearOrphans();
    m_background = BackgroundState();
    m_layers = {{BackgroundLayerId, scene_constants::kBackgroundLayerName, true, true},
                {scene_constants::kFirstUserLayerId, QStringLiteral("图层 1"), true, false}};
    m_activeLayerId = scene_constants::kFirstUserLayerId;
    m_nextLayerId = scene_constants::kFirstUserLayerId + 1;
    m_nextGroupId = 1;
    m_groupParents.clear();
    m_canvasOrigin = QPointF(0.0, 0.0);
    m_exporting = false;
    updateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerId);
    emit contentChanged();
    emit backgroundChanged();
}

void CanvasScene::clearOrphans()
{
    qDeleteAll(m_orphans);
    m_orphans.clear();
}
