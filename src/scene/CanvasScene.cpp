#include "scene/CanvasScene.h"

#include "app/LifecycleLog.h"
#include "items/CanvasItem.h"
#include "scene/Commands.h"
#include "scene/SceneConstants.h"

#include <QStringList>
#include <QUndoStack>

namespace {
// 新建 / 删除命令已经由 AttachItem / DetachItem 写过图元生命周期，不再重复一条“编辑”。
bool IsLifecycleOnlyCommand(const QUndoCommand* command)
{
    if (dynamic_cast<const AddItemCommand*>(command) || dynamic_cast<const RemoveItemsCommand*>(command)) {
        return true;
    }
    if (!command || command->childCount() == 0) {
        return false;
    }
    for (int i = 0; i < command->childCount(); ++i) {
        if (!IsLifecycleOnlyCommand(command->child(i))) {
            return false;
        }
    }
    return true;
}

// 只从命令自带的 before/after 快照里取真正变化的属性；宏命令把子命令的变更拼起来。
QString CommandLogDetail(const QUndoCommand* command, bool reverse)
{
    if (!command) {
        return {};
    }
    if (const auto* geometry = dynamic_cast<const GeometryCommand*>(command)) {
        return geometry->LogDetail(reverse);
    }
    if (const auto* rotation = dynamic_cast<const RotationCommand*>(command)) {
        return rotation->LogDetail(reverse);
    }
    if (const auto* property = dynamic_cast<const ItemPropertyCommand*>(command)) {
        return property->LogDetail(reverse);
    }
    QStringList parts;
    for (int i = 0; i < command->childCount(); ++i) {
        const QString detail = CommandLogDetail(command->child(i), reverse);
        if (!detail.isEmpty() && !parts.contains(detail)) {
            parts.append(detail);
        }
    }
    return parts.join(QStringLiteral("，"));
}
} // namespace

CanvasScene::CanvasScene(QObject* parent) : QGraphicsScene(parent)
{
    m_layerDomain.Reset();
    m_layerDomain.SetLayers({{BackgroundLayerId, scene_constants::BACKGROUND_LAYER_NAME, true, true},
                             {scene_constants::FIRST_USER_LAYER_ID, QStringLiteral("图层 1"), true, false}});
    m_layerDomain.SetActiveLayerId(scene_constants::FIRST_USER_LAYER_ID);

    setBackgroundBrush(Qt::NoBrush);
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    setStickyFocus(true);
    // 选择变化会改写选中黄色轮廓的范围；整场景重绘一次，避免 cosmetic 笔宽
    // 超出图元自身 update 区域留下残影。
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        RefreshSelectionOrder();
        update();
    });
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
    if (m_undoStack) {
        disconnect(m_undoStack, nullptr, this, nullptr);
    }
    m_undoStack = stack;
    if (m_undoStack) {
        // 撤销/重做只恢复被编辑的图元；绑定箭头与容器文字是派生位置，
        // 每次栈位置变化后按当前几何重算，避免停在旧位置。
        connect(m_undoStack, &QUndoStack::indexChanged, this, [this](int index) {
            SyncArrowBindings();
            SyncContainerTexts();
            const int previous = m_lastUndoIndex;
            m_lastUndoIndex = index;
            if (index == previous || !m_undoStack) {
                return;
            }
            const QString command = index > previous ? m_undoStack->text(index - 1) : m_undoStack->text(index);
            const QUndoCommand* commandObject =
                index > previous ? m_undoStack->command(index - 1) : m_undoStack->command(index);
            if (IsLifecycleOnlyCommand(commandObject)) {
                return;
            }
            const bool undoing = index < previous;
            QString detail = command;
            const QString change = CommandLogDetail(commandObject, undoing);
            if (!change.isEmpty()) {
                detail += QStringLiteral("；变更：%1").arg(change);
            }
            const QList<CanvasItem*> selected = SelectedCanvasItems();
            if (!selected.isEmpty()) {
                QStringList names;
                for (int i = 0; i < selected.size() && i < 3; ++i) {
                    names.append(QStringLiteral("uid=%1 %2「%3」")
                                     .arg(selected.at(i)->Uid())
                                     .arg(selected.at(i)->TypeDisplayName(), selected.at(i)->Name()));
                }
                if (selected.size() > 3) {
                    names.append(QStringLiteral("等 %1 个").arg(selected.size()));
                }
                detail += QStringLiteral("；对象：%1").arg(names.join(QStringLiteral("、")));
            }
            LifecycleLog::LogEdit(m_logLabel, undoing ? QStringLiteral("撤销") : QStringLiteral("编辑"), detail);
        });
    }
    m_lastUndoIndex = m_undoStack ? m_undoStack->index() : 0;
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
