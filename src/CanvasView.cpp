#include "CanvasView.h"

#include "CanvasContextMenuController.h"
#include "CanvasItem.h"
#include "CanvasPanController.h"
#include "CanvasResizeController.h"
#include "CanvasScene.h"
#include "CanvasStrokeController.h"
#include "CanvasViewCursor.h"
#include "CanvasViewHitTester.h"
#include "CanvasZoomController.h"
#include "I18n.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

CanvasView::CanvasView(CanvasScene* scene, QWidget* parent) : QGraphicsView(scene, parent), m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, false);
    // QGraphicsView 默认直接用 QGraphicsScenePrivate::drawItems 画图元，不会调用
    // CanvasScene::drawItems 这个虚函数；开启 IndirectPainting 后才会走
    // QGraphicsView::drawItems → QGraphicsScene::drawItems，遮挡裁剪才在真实视图里生效。
    setOptimizationFlag(QGraphicsView::IndirectPainting, true);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignCenter);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setBackgroundBrush(Qt::NoBrush);
    setRubberBandSelectionMode(Qt::ContainsItemShape);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumSize(360, 260);

    m_zoom = std::make_unique<CanvasZoomController>(this, scene);
    m_pan = std::make_unique<CanvasPanController>(this);
    m_resize = std::make_unique<CanvasResizeController>(this, scene);
    m_stroke = std::make_unique<CanvasStrokeController>(this, scene);
    m_hits = std::make_unique<CanvasViewHitTester>(this, scene);
    m_cursor = std::make_unique<CanvasViewCursor>(this);
    m_menu = std::make_unique<CanvasContextMenuController>(this, scene);

    updateCursor();
}

qreal CanvasView::zoom() const
{
    return m_zoom->zoom();
}

void CanvasView::setMode(ToolMode mode)
{
    if (m_mode == mode) {
        return;
    }

    finishCurrentStroke();
    m_mode = mode;
    m_scene->setMode(mode);

    switch (mode) {
    case ToolMode::View:
        setDragMode(QGraphicsView::NoDrag);
        m_scene->clearSelection();
        fitAll();
        break;
    case ToolMode::Brush:
        setDragMode(QGraphicsView::NoDrag);
        m_scene->clearSelection();
        break;
    case ToolMode::Edit:
        setDragMode(QGraphicsView::RubberBandDrag);
        break;
    case ToolMode::Text:
        setDragMode(QGraphicsView::NoDrag);
        m_scene->clearSelection();
        break;
    }

    updateCursor();
    emit modeChanged(mode);
}

void CanvasView::fitAll()
{
    m_zoom->fitAll();
}

void CanvasView::zoomIn()
{
    m_zoom->zoomIn();
}

void CanvasView::zoomOut()
{
    m_zoom->zoomOut();
}

void CanvasView::actualSize()
{
    m_zoom->actualSize();
}

void CanvasView::setBrushColor(const QColor& color)
{
    m_stroke->setBrushColor(color);
}

void CanvasView::setBrushWidth(qreal width)
{
    m_stroke->setBrushWidth(width);
}

void CanvasView::setStrokeSmoothing(qreal tolerance)
{
    m_stroke->setStrokeSmoothing(tolerance);
}

void CanvasView::addContextMenuGroup(const QString& title, const QList<QAction*>& actions)
{
    m_menu->addGroup(title, actions);
}

void CanvasView::setCanvasColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_canvasColor = color;
    viewport()->update();
}

void CanvasView::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, m_canvasColor);
    QGraphicsView::drawBackground(painter, rect);
}

void CanvasView::updateCursor()
{
    m_cursor->refresh(m_mode, m_pan->isPanning());
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
    m_zoom->handleWheel(event);
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    // 右键点中已选图元时不要走默认选择逻辑：QGraphicsView 会先把多选收成单选，
    // 导致随后的右键菜单拿不到完整选择，“组合选中图元”被误禁用。
    if (m_mode == ToolMode::Edit && event->button() == Qt::RightButton) {
        CanvasItem* item = m_hits->editableItemAt(position);
        if (item && item->isSelected()) {
            event->accept();
            return;
        }
    }

    // 平移只保留中键与查看模式左键：编辑模式不再用 Ctrl+左键平移，把 Ctrl 留给多选。
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && (m_mode == ToolMode::View || m_spaceDown))) {
        m_pan->beginPan(position);
        event->accept();
        return;
    }

    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton && m_resize->beginResize(position)) {
        event->accept();
        return;
    }

    // 需求 15：重叠图元优先命中图层面板里已选图层中的图元。
    // QGraphicsScene 的默认选择总是选最上层，这里在按下期间临时让“非优先图层”
    // 的图元不可选中，交给基类完成选择/拖拽后再恢复，避免改变真实可见顺序。
    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton) {
        CanvasItem* preferred = m_hits->preferredEditableItemAt(position);
        const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(position));
        const bool preferredIsTop =
            !candidates.isEmpty() && qgraphicsitem_cast<CanvasItem*>(candidates.first()) == preferred;
        if (preferred && !preferredIsTop) {
            struct SuspendedItem {
                CanvasItem* item;
                Qt::MouseButtons buttons;
                bool selectable;
            };
            QList<SuspendedItem> suspended;
            for (QGraphicsItem* graphicsItem : candidates) {
                auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
                if (!canvasItem || canvasItem == preferred || !canvasItem->isVisible()) {
                    continue;
                }
                if (!m_scene->isItemEditable(canvasItem)) {
                    continue;
                }
                if (m_scene->preferredLayerIds().contains(canvasItem->layerId())) {
                    continue;
                }
                suspended.append({canvasItem, canvasItem->acceptedMouseButtons(),
                                  canvasItem->flags().testFlag(QGraphicsItem::ItemIsSelectable)});
                canvasItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
                canvasItem->setAcceptedMouseButtons(Qt::NoButton);
            }
            QGraphicsView::mousePressEvent(event);
            for (const SuspendedItem& entry : suspended) {
                entry.item->setAcceptedMouseButtons(entry.buttons);
                entry.item->setFlag(QGraphicsItem::ItemIsSelectable, entry.selectable);
            }
            return;
        }
    }

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::Brush && !m_stroke->hasActiveStroke()) {
        if (!m_stroke->beginStroke(position)) {
            event->accept();
            return;
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::Text) {
        if (!m_scene->isActiveLayerWritable()) {
            emit statusMessage(tr("The current layer is locked or hidden; text cannot be placed. Unlock or switch "
                                  "layers in the layer panel first."));
            event->accept();
            return;
        }
        emit textPlacementRequested(mapToScene(position));
        event->accept();
        return;
    }

    // 分组层级：单击组内任意图元时先展开成整棵组树的选择，再交给基类开始拖拽，
    // 这样一拖就是整组移动；Ctrl 点击仍保留加选/减选。
    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
        CanvasItem* item = m_hits->editableItemAt(position);
        if (item && item->groupId() > 0) {
            m_scene->selectGroupOfItem(item);
        }
    }

    QGraphicsView::mousePressEvent(event);

    // 基类按下时可能把多选收成单选，按完再补一次整组选择。
    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
        CanvasItem* item = m_hits->editableItemAt(position);
        if (item && item->groupId() > 0 && m_scene->selectedCanvasItems().size() > 1) {
            m_scene->selectGroupOfItem(item);
        }
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();

    if (m_pan->movePan(position)) {
        event->accept();
        return;
    }

    if (m_resize->isResizing()) {
        m_resize->updateResize(mapToScene(position));
        event->accept();
        return;
    }

    if (m_stroke->hasActiveStroke()) {
        m_stroke->appendPoint(position);
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
    m_resize->updateHoverCursor(position, m_pan->isPanning(), m_stroke->hasActiveStroke());
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pan->isPanning()) {
        m_pan->endPan();
        event->accept();
        return;
    }

    if (m_resize->isResizing() && event->button() == Qt::LeftButton) {
        m_resize->endResize();
        event->accept();
        return;
    }

    if (m_stroke->hasActiveStroke() && event->button() == Qt::LeftButton) {
        finishCurrentStroke();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // 需求 42：属性窗在查看/画笔/文字模式下也能打开（只读）；
        // 编辑模式仍要求图元可编辑，锁定图层维持原来的“先解锁”提示。
        if (m_mode == ToolMode::Edit) {
            CanvasItem* canvasItem = m_hits->editableItemAt(event->position().toPoint());
            if (!canvasItem) {
                if (m_hits->topItemAt(event->position().toPoint())) {
                    emit statusMessage(tr("The layer containing this item is locked or hidden; editing is unavailable. "
                                          "Unlock or show that layer in the layer panel first."));
                    event->accept();
                    return;
                }
                QGraphicsView::mouseDoubleClickEvent(event);
                return;
            }
            emit itemPropertiesRequested(canvasItem);
            event->accept();
            return;
        }

        CanvasItem* canvasItem = m_hits->propertyItemAt(event->position().toPoint());
        if (canvasItem) {
            emit itemPropertiesRequested(canvasItem);
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CanvasView::contextMenuEvent(QContextMenuEvent* event)
{
    m_menu->showMenu(event);
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        if (!m_pan->isPanning()) {
            viewport()->setCursor(Qt::OpenHandCursor);
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && m_stroke->hasActiveStroke()) {
        m_stroke->cancelStroke();
        emit statusMessage(tr("Current stroke canceled"));
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        updateCursor();
        event->accept();
        return;
    }
    if (!m_pan->isPanning()) {
        m_spaceDown = false;
        updateCursor();
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::focusOutEvent(QFocusEvent* event)
{
    // 焦点离开后收不到按键抬起事件，按住状态必须复位，否则回来后 Ctrl / 空格会被误判为一直按住。
    if (!m_pan->isPanning()) {
        m_spaceDown = false;
        updateCursor();
    }
    QGraphicsView::focusOutEvent(event);
}

void CanvasView::finishCurrentStroke()
{
    m_stroke->finishStroke();
}

void CanvasView::cancelCurrentStroke()
{
    m_stroke->cancelStroke();
}

void CanvasView::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
    if (m_mode != ToolMode::Edit) {
        return;
    }
    QPainter painter(viewport());
    m_resize->drawHandles(&painter);
}

void CanvasView::leaveEvent(QEvent* event)
{
    m_resize->resetHoverOnLeave();
    QGraphicsView::leaveEvent(event);
}
