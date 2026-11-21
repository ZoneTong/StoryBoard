#include "CanvasContextMenuController.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QKeySequence>
#include <QMenu>

namespace {
QString trView(const char* text)
{
    return QCoreApplication::translate("CanvasView", text);
}
} // namespace

CanvasContextMenuController::CanvasContextMenuController(CanvasView* view, CanvasScene* scene)
    : m_view(view), m_scene(scene)
{
}

void CanvasContextMenuController::addGroup(const QString& title, const QList<QAction*>& actions)
{
    for (const ContextMenuGroup& group : std::as_const(m_groups)) {
        if (group.title == title) {
            return;
        }
    }
    m_groups.append({title, actions});
}

void CanvasContextMenuController::showMenu(QContextMenuEvent* event)
{
    if (m_view->mode() != ToolMode::EDIT) {
        m_view->baseContextMenuEvent(event);
        return;
    }

    CanvasItem* item = m_view->m_hits->editableItemAt(event->pos());
    if (!item) {
        if (m_view->m_hits->topItemAt(event->pos())) {
            emit m_view->statusMessage(trView("The layer containing this item is locked or hidden; editing is "
                                              "unavailable. Unlock or show that layer in the layer panel first."));
            event->accept();
            return;
        }
        m_view->baseContextMenuEvent(event);
        return;
    }

    if (!item->isSelected()) {
        m_scene->clearSelection();
        item->setSelected(true);
    }

    const CanvasLayer itemLayer = m_scene->layer(item->layerId());
    const int layerIndex = m_scene->layerIndex(item->layerId());

    QMenu menu(m_view);
    QAction* title =
        menu.addAction(trView("%1 · %2 · height %3").arg(item->name(), itemLayer.name).arg(item->height()));
    title->setEnabled(false);
    menu.addSeparator();

    // 需求 42：属性窗在任意模式都可打开；编辑模式下此入口与双击/Q 等价。
    QAction* properties = menu.addAction(trView("Item properties…"));
    QAction* duplicate = menu.addAction(trView("Duplicate item"));
    duplicate->setEnabled(item->isSelected());
    QAction* smooth = menu.addAction(trView("Simplify curve"));
    smooth->setEnabled(m_scene->canSmoothSelectedStrokes());
    QAction* group = menu.addAction(trView("Group selected items"));
    group->setEnabled(m_scene->canGroupSelectedItems());
    QAction* ungroup = menu.addAction(trView("Ungroup selected items"));
    ungroup->setEnabled(m_scene->canUngroupSelectedItems());

    // 主窗口挂进来的动作组（例如布尔运算）与菜单栏、工具栏共用同一批 QAction。
    for (const ContextMenuGroup& group : std::as_const(m_groups)) {
        if (group.actions.isEmpty()) {
            continue;
        }
        QMenu* submenu = menu.addMenu(group.title);
        for (QAction* action : group.actions) {
            submenu->addAction(action);
        }
    }
    menu.addSeparator();

    // 右键菜单的高度项只写动作名，快捷键交给 QAction 自己画，避免跟菜单里的快捷键重复。
    QAction* heightUp = menu.addAction(trView("Height +1 (stack up)"));
    QAction* heightDown = menu.addAction(trView("Height -1 (stack down)"));
    heightUp->setShortcut(QKeySequence(Qt::Key_BracketRight));
    heightDown->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    heightUp->setShortcutVisibleInContextMenu(true);
    heightDown->setShortcutVisibleInContextMenu(true);
    heightDown->setEnabled(item->height() > 0);
    menu.addSeparator();

    QAction* layerUp = menu.addAction(trView("Move to the layer above"));
    QAction* layerDown = menu.addAction(trView("Move to the layer below"));
    layerUp->setEnabled(layerIndex >= 0 && layerIndex < m_scene->layerCount() - 1);
    layerDown->setEnabled(layerIndex > 1);
    menu.addSeparator();

    QAction* remove = menu.addAction(trView("Delete item"));
    remove->setEnabled(item->isSelected());

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) {
        return;
    }

    if (chosen == properties) {
        emit m_view->itemPropertiesRequested(item);
    } else if (chosen == duplicate) {
        const int duplicated = m_scene->duplicateSelectedItems();
        if (duplicated > 0) {
            emit m_view->statusMessage(trView("Duplicated %1 item(s)").arg(duplicated));
        }
    } else if (chosen == smooth) {
        const int smoothed = m_scene->smoothSelectedStrokes(m_view->strokeSmoothing());
        if (smoothed > 0) {
            emit m_view->statusMessage(trView("Simplified %1 curve(s)").arg(smoothed));
        }
    } else if (chosen == group) {
        const int grouped = m_scene->groupSelectedItems();
        if (grouped > 0) {
            emit m_view->statusMessage(trView("Grouped %1 item(s)").arg(grouped));
        }
    } else if (chosen == ungroup) {
        const int ungrouped = m_scene->ungroupSelectedItems();
        if (ungrouped > 0) {
            emit m_view->statusMessage(trView("Ungrouped %1 item(s)").arg(ungrouped));
        }
    } else if (chosen == heightUp) {
        m_scene->adjustSelectedHeights(1);
    } else if (chosen == heightDown) {
        m_scene->adjustSelectedHeights(-1);
    } else if (chosen == layerUp) {
        m_scene->moveSelectedItemsToAdjacentLayer(1);
    } else if (chosen == layerDown) {
        m_scene->moveSelectedItemsToAdjacentLayer(-1);
    } else if (chosen == remove) {
        m_scene->deleteSelected();
    }

    event->accept();
}
