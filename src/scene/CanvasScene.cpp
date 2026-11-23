#include "scene/CanvasScene.h"

#include "items/CanvasItem.h"
#include "scene/SceneConstants.h"

#include <QUndoStack>

CanvasScene::CanvasScene(QObject* parent) : QGraphicsScene(parent)
{
    m_layerDomain.Reset();
    m_layerDomain.SetLayers({{BackgroundLayerId, scene_constants::BACKGROUND_LAYER_NAME, true, true},
                             {scene_constants::FIRST_USER_LAYER_ID, QStringLiteral("图层 1"), true, false}});
    m_layerDomain.SetActiveLayerId(scene_constants::FIRST_USER_LAYER_ID);

    setBackgroundBrush(Qt::NoBrush);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    setStickyFocus(true);
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() { RefreshSelectionOrder(); });
    UpdateSceneExtent();
}

QList<CanvasItem*> CanvasScene::SelectionOrder() const
{
    QList<CanvasItem*> order;
    order.reserve(m_selectionOrder.size());
    for (const QPointer<CanvasItem>& item : m_selectionOrder) {
        if (item) {
            order.append(item.data());
        }
    }
    return order;
}

void CanvasScene::RefreshSelectionOrder()
{
    QSet<CanvasItem*> current;
    const QList<CanvasItem*> selected = SelectedCanvasItems();
    for (CanvasItem* item : selected) {
        current.insert(item);
    }
    for (int i = m_selectionOrder.size() - 1; i >= 0; --i) {
        if (!m_selectionOrder.at(i) || !current.contains(m_selectionOrder.at(i).data())) {
            m_selectionOrder.removeAt(i);
        }
    }
    QSet<CanvasItem*> known;
    for (int i = 0; i < m_selectionOrder.size(); ++i) {
        if (m_selectionOrder.at(i)) {
            known.insert(m_selectionOrder.at(i).data());
        }
    }
    // selectedCanvasItems 按场景叠放顺序返回，因此同一批框选进来的图元
    // 天然是「最底层在前」，正好满足剪除以最底层为底的要求。
    for (CanvasItem* item : selected) {
        if (!known.contains(item)) {
            m_selectionOrder.append(item);
        }
    }
}

void CanvasScene::ForgetItem(CanvasItem* item)
{
    for (int i = m_selectionOrder.size() - 1; i >= 0; --i) {
        if (m_selectionOrder.at(i).data() == item) {
            m_selectionOrder.removeAt(i);
        }
    }
}

CanvasScene::~CanvasScene()
{
    // 场景销毁时 Qt 会逐一析构图元；选中图元被析构会发出 selectionChanged，
    // 而槽内扫描排序缓存会触碰同批析构中图元的 QPointer 观测数据——此阶段
    // 数据已不可靠。先断开信号并清空缓存，再交由基类清理图元。
    disconnect(this, &QGraphicsScene::selectionChanged, this, nullptr);
    m_selectionOrder.clear();
    ClearOrphans();
}

void CanvasScene::SetUndoStack(QUndoStack* stack)
{
    m_undoStack = stack;
}

void CanvasScene::SetMode(ToolMode mode)
{
    if (m_mode == mode) {
        return;
    }

    if (m_mode == ToolMode::EDIT) {
        clearSelection();
    }

    m_mode = mode;
    UpdateItemFlags();
    update();
    emit modeChanged(mode);
}

void CanvasScene::ResetDocument()
{
    if (m_undoStack) {
        m_undoStack->clear();
    }

    clear();
    ClearOrphans();
    m_selectionOrder.clear();
    m_background = CanvasBackground();
    m_layerDomain.Reset();
    m_layerDomain.SetLayers({{BackgroundLayerId, scene_constants::BACKGROUND_LAYER_NAME, true, true},
                             {scene_constants::FIRST_USER_LAYER_ID, QStringLiteral("图层 1"), true, false}});
    m_layerDomain.SetActiveLayerId(scene_constants::FIRST_USER_LAYER_ID);
    m_groups.Reset();
    m_canvasOrigin = QPointF(0.0, 0.0);
    m_exporting = false;
    UpdateSceneExtent();
    update();
    emit layersChanged();
    emit activeLayerChanged(m_layerDomain.ActiveLayerId());
    emit contentChanged();
    emit backgroundChanged();
}

void CanvasScene::ClearOrphans()
{
    qDeleteAll(m_orphans);
    m_orphans.clear();
}
