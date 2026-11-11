#include "I18n.h"

#include "CanvasView.h"

#include "CanvasItem.h"
#include "CanvasScene.h"
#include "StrokeItem.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFocusEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr qreal kMinZoom = 0.02;
constexpr qreal kMaxZoom = 40.0;
const qreal kFitMaxZoom = 3.0;
} // namespace

CanvasView::CanvasView(CanvasScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                   | QPainter::SmoothPixmapTransform);
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
    updateCursor();
}

qreal CanvasView::zoom() const
{
    return transform().m11();
}

void CanvasView::setMode(ToolMode mode)
{
    if (m_mode == mode)
        return;

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
    if (viewport()->width() <= 8 || viewport()->height() <= 8)
        return;

    QRectF target = m_scene->contentRect();
    if (target.isEmpty())
        target = m_scene->canvasRect();

    const qreal margin = qMax<qreal>(24.0, qMax(target.width(), target.height()) * 0.035);
    target = target.adjusted(-margin, -margin, margin, margin);

    fitInView(target, Qt::KeepAspectRatio);

    const qreal fitted = transform().m11();
    const bool tooLarge = fitted > kFitMaxZoom;
    const bool tooSmall = fitted < kMinZoom;
    if (tooLarge || tooSmall) {
        const qreal clamped = qBound(kMinZoom, fitted, kFitMaxZoom);
        setTransformationAnchor(QGraphicsView::NoAnchor);
        setTransform(QTransform::fromScale(clamped, clamped));
        centerOn(target.center());
    }

    emitZoom();
}

void CanvasView::zoomIn()
{
    applyScale(1.22);
}

void CanvasView::zoomOut()
{
    applyScale(1.0 / 1.22);
}

void CanvasView::actualSize()
{
    const QPointF center = mapToScene(viewport()->rect().center());
    setTransformationAnchor(QGraphicsView::NoAnchor);
    resetTransform();
    centerOn(center);
    emitZoom();
}

void CanvasView::applyScale(qreal factor)
{
    if (factor <= 0.0)
        return;

    const qreal current = zoom();
    if (current <= 0.0)
        return;

    const qreal target = qBound(kMinZoom, current * factor, kMaxZoom);
    const qreal applied = target / current;
    if (qFuzzyCompare(applied, 1.0))
        return;

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    scale(applied, applied);
    emitZoom();
}

void CanvasView::emitZoom()
{
    if (m_scene)
        m_scene->refreshViewDependentGeometry();
    emit zoomChanged(zoom());
}

void CanvasView::setBrushColor(const QColor& color)
{
    if (color.isValid())
        m_brushColor = color;
}

void CanvasView::setBrushWidth(qreal width)
{
    m_brushWidth = qBound<qreal>(0.5, width, 64.0);
}

void CanvasView::setStrokeSmoothing(int tolerance)
{
    m_strokeSmoothing = qBound(0, tolerance, 30);
}

void CanvasView::setCanvasColor(const QColor& color)
{
    if (!color.isValid())
        return;
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
    if (m_panning) {
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (ctrlPanActive()) {
        // 编辑模式按住 Ctrl 时左键让位给整体视图平移，画板边缘不再提示可拉伸。
        m_hoverHandle = CanvasHandle::None;
        viewport()->setCursor(Qt::OpenHandCursor);
        return;
    }
    switch (m_mode) {
    case ToolMode::View:
        viewport()->setCursor(Qt::OpenHandCursor);
        break;
    case ToolMode::Brush:
        viewport()->setCursor(Qt::CrossCursor);
        break;
    case ToolMode::Edit:
        viewport()->setCursor(Qt::ArrowCursor);
        break;
    case ToolMode::Text:
        viewport()->setCursor(Qt::IBeamCursor);
        break;
    }
}

bool CanvasView::ctrlPanActive() const
{
    return ctrlPanActive(m_ctrlDown ? Qt::ControlModifier : Qt::NoModifier);
}

bool CanvasView::ctrlPanActive(Qt::KeyboardModifiers modifiers) const
{
    return m_mode == ToolMode::Edit && modifiers.testFlag(Qt::ControlModifier);
}

void CanvasView::beginPan(const QPoint& position)
{
    m_panning = true;
    m_panStart = position;
    m_panCenterScene = mapToScene(viewport()->rect()).boundingRect().center();
    m_panByCtrl = false;
    m_panMoved = false;
    m_panClickItem = nullptr;
    viewport()->setCursor(Qt::ClosedHandCursor);
}

void CanvasView::endPan()
{
    m_panning = false;
    m_panByCtrl = false;
    m_panMoved = false;
    m_panClickItem = nullptr;
    updateCursor();
}

void CanvasView::applyPanClickSelection(const QPoint& viewportPos)
{
    // Ctrl 点击未拖动时按点选处理，保持 CanvasItem 原本“Ctrl 点击切换选中”的行为：
    // 空白处点击保留现有选择，图元上点击则在选中/未选中之间切换。
    CanvasItem* item = m_panClickItem;
    if (!item)
        return;
    if (!m_scene->isItemEditable(item))
        return;
    item->setSelected(!item->isSelected());
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const qreal current = zoom();
    qreal factor = qPow(1.0015, delta);
    const qreal target = qBound(kMinZoom, current * factor, kMaxZoom);
    factor = target / current;

    if (qFuzzyCompare(factor, 1.0)) {
        event->accept();
        return;
    }

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(factor, factor);
    emitZoom();
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    m_ctrlDown = event->modifiers().testFlag(Qt::ControlModifier);
    // 右键点中已选图元时不要走默认选择逻辑：QGraphicsView 会先把多选收成单选，
    // 导致随后的右键菜单拿不到完整选择，“组合选中图元”被误禁用。
    if (m_mode == ToolMode::Edit && event->button() == Qt::RightButton) {
        CanvasItem* item = editableItemAt(position);
        if (item && item->isSelected()) {
            event->accept();
            return;
        }
    }

    // 编辑模式下 Ctrl + 左键拖动平移整体视图：先于图元选择与画板边缘拉伸处理。
    const bool ctrlPan = m_mode == ToolMode::Edit && event->button() == Qt::LeftButton
                         && event->modifiers().testFlag(Qt::ControlModifier);
    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton
            && (m_mode == ToolMode::View || m_spaceDown || ctrlPan))) {
        beginPan(position);
        if (ctrlPan) {
            m_panByCtrl = true;
            m_panClickItem = editableItemAt(position);
        }
        event->accept();
        return;
    }

    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton
        && beginCanvasResize(position)) {
        event->accept();
        return;
    }

    // 需求 15：重叠图元优先命中图层面板里已选图层中的图元。
    // QGraphicsScene 的默认选择总是选最上层，这里在按下期间临时让“非优先图层”
    // 的图元不可选中，交给基类完成选择/拖拽后再恢复，避免改变真实可见顺序。
    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton) {
        CanvasItem* preferred = preferredEditableItemAt(position);
        const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(position));
        const bool preferredIsTop = !candidates.isEmpty()
            && qgraphicsitem_cast<CanvasItem*>(candidates.first()) == preferred;
        if (preferred && !preferredIsTop) {
            struct SuspendedItem {
                CanvasItem* item;
                Qt::MouseButtons buttons;
                bool selectable;
            };
            QList<SuspendedItem> suspended;
            for (QGraphicsItem* graphicsItem : candidates) {
                auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
                if (!canvasItem || canvasItem == preferred || !canvasItem->isVisible())
                    continue;
                if (!m_scene->isItemEditable(canvasItem))
                    continue;
                if (m_scene->preferredLayerIds().contains(canvasItem->layerId()))
                    continue;
                suspended.append({ canvasItem,
                                   canvasItem->acceptedMouseButtons(),
                                   canvasItem->flags().testFlag(QGraphicsItem::ItemIsSelectable) });
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

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::Brush && !m_activeStroke) {
        if (!m_scene->isActiveLayerWritable()) {
            emit statusMessage(uiText(QStringLiteral(
                "The current layer is locked or hidden; drawing is unavailable. Unlock or switch layers in the layer panel first.")));
            event->accept();
            return;
        }
        auto* stroke = new StrokeItem();
        // Keep the in-progress stroke on top while drawing. The final layer
        // and height are assigned when the stroke is committed.
        stroke->setZValue(1.0e12);
        stroke->beginStroke(mapToScene(position), m_brushColor, m_brushWidth);
        m_scene->addItem(stroke);
        m_activeStroke = stroke;
        m_scene->clearSelection();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::Text) {
        if (!m_scene->isActiveLayerWritable()) {
            emit statusMessage(uiText(QStringLiteral(
                "The current layer is locked or hidden; text cannot be placed. Unlock or switch layers in the layer panel first.")));
            event->accept();
            return;
        }
        emit textPlacementRequested(mapToScene(position));
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    m_ctrlDown = event->modifiers().testFlag(Qt::ControlModifier);

    if (m_panning) {
        if (m_panByCtrl && !m_panMoved
            && (position - m_panStart).manhattanLength() > QApplication::startDragDistance()) {
            m_panMoved = true;
        }
        // 用“按下点 → 当前点”的场景位移来平移：两个点在同一时刻的变换下相减，
        // 结果与当前视图位置无关。若拿按下时的场景锚点去和当前 mapToScene 相减，
        // 视图每移动一次都会把上次的位移重复计入，画面就会来回抖动。
        const QPointF sceneDelta = mapToScene(position) - mapToScene(m_panStart);
        centerOn(m_panCenterScene - sceneDelta);
        event->accept();
        return;
    }

    if (m_resizingCanvas) {
        updateCanvasResize(mapToScene(position));
        event->accept();
        return;
    }

    if (m_activeStroke) {
        m_activeStroke->appendScenePoint(mapToScene(position));
        viewport()->update();
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
    updateCanvasHoverCursor(position);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning) {
        // Ctrl 按住但几乎没有拖动时按点选处理，免得平移把 Ctrl 点击多选的既有行为挤掉。
        if (m_panByCtrl && !m_panMoved)
            applyPanClickSelection(m_panStart);
        endPan();
        event->accept();
        return;
    }

    if (m_resizingCanvas && event->button() == Qt::LeftButton) {
        endCanvasResize();
        event->accept();
        return;
    }

    if (m_activeStroke && event->button() == Qt::LeftButton) {
        finishCurrentStroke();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // 编辑模式下 Ctrl 是平移修饰键，按住 Ctrl 双击不打开属性窗。
    if (m_mode == ToolMode::Edit && event->modifiers().testFlag(Qt::ControlModifier)) {
        QGraphicsView::mouseDoubleClickEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // 需求 42：属性窗在查看/画笔/文字模式下也能打开（只读）；
        // 编辑模式仍要求图元可编辑，锁定图层维持原来的“先解锁”提示。
        if (m_mode == ToolMode::Edit) {
            CanvasItem* canvasItem = editableItemAt(event->position().toPoint());
            if (!canvasItem) {
                if (topCanvasItemAt(event->position().toPoint())) {
                    emit statusMessage(uiText(QStringLiteral(
                        "The layer containing this item is locked or hidden; editing is unavailable. Unlock or show that layer in the layer panel first.")));
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

        CanvasItem* canvasItem = propertyItemAt(event->position().toPoint());
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
    if (m_mode != ToolMode::Edit) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    CanvasItem* item = editableItemAt(event->pos());
    if (!item) {
        if (topCanvasItemAt(event->pos())) {
            emit statusMessage(uiText(QStringLiteral(
                "The layer containing this item is locked or hidden; editing is unavailable. Unlock or show that layer in the layer panel first.")));
            event->accept();
            return;
        }
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    if (!item->isSelected()) {
        m_scene->clearSelection();
        item->setSelected(true);
    }

    const CanvasLayer itemLayer = m_scene->layer(item->layerId());
    const int layerIndex = m_scene->layerIndex(item->layerId());

    QMenu menu(this);
    QAction* title = menu.addAction(
        uiText(QStringLiteral("%1 · %2 · height %3"))
            .arg(item->name(), itemLayer.name)
            .arg(item->height()));
    title->setEnabled(false);
    menu.addSeparator();

    // 需求 42：属性窗在任意模式都可打开；编辑模式下此入口与双击/Q 等价。
    QAction* properties = menu.addAction(uiText(QStringLiteral("Item properties…")));
    QAction* duplicate = menu.addAction(uiText(QStringLiteral("Duplicate item")));
    duplicate->setEnabled(item->isSelected());
    QAction* smooth = menu.addAction(uiText(QStringLiteral("Smooth strokes")));
    smooth->setEnabled(m_scene->canSmoothSelectedStrokes());
    QAction* group = menu.addAction(uiText(QStringLiteral("Group selected items")));
    group->setEnabled(m_scene->canGroupSelectedItems());
    QAction* ungroup = menu.addAction(uiText(QStringLiteral("Ungroup selected items")));
    ungroup->setEnabled(m_scene->canUngroupSelectedItems());
    menu.addSeparator();

    QAction* heightUp = menu.addAction(uiText(QStringLiteral("Height +1 (stack up)    ]")));
    QAction* heightDown = menu.addAction(uiText(QStringLiteral("Height -1 (stack down)    [")));
    heightDown->setEnabled(item->height() > 0);
    menu.addSeparator();

    QAction* layerUp = menu.addAction(uiText(QStringLiteral("Move to the layer above")));
    QAction* layerDown = menu.addAction(uiText(QStringLiteral("Move to the layer below")));
    layerUp->setEnabled(layerIndex >= 0 && layerIndex < m_scene->layerCount() - 1);
    layerDown->setEnabled(layerIndex > 1);
    menu.addSeparator();

    QAction* remove = menu.addAction(uiText(QStringLiteral("Delete item")));
    remove->setEnabled(item->isSelected());

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;

    if (chosen == properties)
        emit itemPropertiesRequested(item);
    else if (chosen == duplicate) {
        const int duplicated = m_scene->duplicateSelectedItems();
        if (duplicated > 0)
            emit statusMessage(uiText(QStringLiteral("Duplicated %1 item(s)")).arg(duplicated));
    } else if (chosen == smooth) {
        const int smoothed = m_scene->smoothSelectedStrokes(m_strokeSmoothing);
        if (smoothed > 0)
            emit statusMessage(uiText(QStringLiteral("Smoothed %1 stroke(s)")).arg(smoothed));
    } else if (chosen == group) {
        const int grouped = m_scene->groupSelectedItems();
        if (grouped > 0)
            emit statusMessage(uiText(QStringLiteral("Grouped %1 item(s)")).arg(grouped));
    } else if (chosen == ungroup) {
        const int ungrouped = m_scene->ungroupSelectedItems();
        if (ungrouped > 0)
            emit statusMessage(uiText(QStringLiteral("Ungrouped %1 item(s)")).arg(ungrouped));
    } else if (chosen == heightUp)
        m_scene->adjustSelectedHeights(1);
    else if (chosen == heightDown)
        m_scene->adjustSelectedHeights(-1);
    else if (chosen == layerUp)
        m_scene->moveSelectedItemsToAdjacentLayer(1);
    else if (chosen == layerDown)
        m_scene->moveSelectedItemsToAdjacentLayer(-1);
    else if (chosen == remove)
        m_scene->deleteSelected();

    event->accept();
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        if (!m_panning)
            viewport()->setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Control && !event->isAutoRepeat() && m_mode == ToolMode::Edit) {
        // 编辑模式按住 Ctrl：左键改为平移整体视图，鼠标暂时不动也要把光标换成手形。
        m_ctrlDown = true;
        if (!m_panning)
            updateCursor();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && m_activeStroke) {
        cancelCurrentStroke();
        emit statusMessage(uiText(QStringLiteral("Current stroke canceled")));
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
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat()) {
        m_ctrlDown = false;
        if (!m_panning && m_mode == ToolMode::Edit)
            updateCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::focusOutEvent(QFocusEvent* event)
{
    // 焦点离开后收不到按键抬起事件，按住状态必须复位，否则回来后 Ctrl / 空格会被误判为一直按住。
    if (!m_panning) {
        m_ctrlDown = false;
        m_spaceDown = false;
        updateCursor();
    }
    QGraphicsView::focusOutEvent(event);
}

void CanvasView::finishCurrentStroke()
{
    if (!m_activeStroke)
        return;

    StrokeItem* stroke = m_activeStroke;
    m_activeStroke = nullptr;
    stroke->endStroke(m_strokeSmoothing);

    if (stroke->isEmpty()) {
        m_scene->removeItem(stroke);
        delete stroke;
        updateCursor();
        return;
    }

    m_scene->commitNewItem(stroke, uiText(QStringLiteral("Add stroke")));
    updateCursor();
}

void CanvasView::cancelCurrentStroke()
{
    if (!m_activeStroke)
        return;

    StrokeItem* stroke = m_activeStroke;
    m_activeStroke = nullptr;
    m_scene->removeItem(stroke);
    delete stroke;
    updateCursor();
}

void CanvasView::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
    if (m_mode != ToolMode::Edit)
        return;
    QPainter painter(viewport());
    drawCanvasHandles(&painter);
}

void CanvasView::leaveEvent(QEvent* event)
{
    if (!m_resizingCanvas) {
        m_hoverHandle = CanvasHandle::None;
        updateCursor();
    }
    QGraphicsView::leaveEvent(event);
}

CanvasItem* CanvasView::topCanvasItemAt(const QPoint& viewportPos) const
{
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (canvasItem && canvasItem->isVisible())
            return canvasItem;
    }
    return nullptr;
}

CanvasItem* CanvasView::propertyItemAt(const QPoint& viewportPos) const
{
    // 只按可见性和真实叠放顺序命中，不检查当前工具模式或图层锁定状态；
    // 属性窗会决定自己是否可编辑。
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (canvasItem && canvasItem->isVisible())
            return canvasItem;
    }
    return nullptr;
}

CanvasItem* CanvasView::preferredEditableItemAt(const QPoint& viewportPos) const
{
    const QList<int> preferred = m_scene->preferredLayerIds();
    if (preferred.isEmpty())
        return nullptr;

    // 保持顶层优先：列表中越靠前叠得越高。
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (!canvasItem || !canvasItem->isVisible())
            continue;
        if (preferred.contains(canvasItem->layerId()) && m_scene->isItemEditable(canvasItem))
            return canvasItem;
    }
    return nullptr;
}

CanvasItem* CanvasView::editableItemAt(const QPoint& viewportPos) const
{
    // 命中要“穿透”锁定或隐藏的图层：自上而下找第一个真正可编辑的图元，
    // 这样锁定的顶层图元不会挡住下面未锁定图层上的图元。
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    const QList<int> preferred = m_scene->preferredLayerIds();
    CanvasItem* fallback = nullptr;
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (!canvasItem || !canvasItem->isVisible())
            continue;
        if (!m_scene->isItemEditable(canvasItem))
            continue;
        // 重叠区域优先命中图层面板中已选图层里的图元，其余仍按叠放顺序取最上层。
        if (preferred.contains(canvasItem->layerId()))
            return canvasItem;
        if (!fallback)
            fallback = canvasItem;
    }
    return fallback;
}

Qt::CursorShape CanvasView::canvasHandleCursor(CanvasHandle handle)
{
    switch (handle) {
    case CanvasHandle::Left:
    case CanvasHandle::Right:
        return Qt::SizeHorCursor;
    case CanvasHandle::Top:
    case CanvasHandle::Bottom:
        return Qt::SizeVerCursor;
    case CanvasHandle::TopLeft:
    case CanvasHandle::BottomRight:
        return Qt::SizeFDiagCursor;
    case CanvasHandle::TopRight:
    case CanvasHandle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case CanvasHandle::None:
        break;
    }
    return Qt::ArrowCursor;
}

CanvasView::CanvasHandle CanvasView::canvasHandleAt(const QPoint& viewportPos) const
{
    if (m_mode != ToolMode::Edit)
        return CanvasHandle::None;

    const QRectF rect = mapFromScene(m_scene->canvasRect()).boundingRect();
    const QPointF point = viewportPos;
    const qreal tolerance = 7.0;
    const QRectF outer = rect.adjusted(-tolerance, -tolerance, tolerance, tolerance);
    if (!outer.contains(point))
        return CanvasHandle::None;

    const bool left = qAbs(point.x() - rect.left()) <= tolerance;
    const bool right = qAbs(point.x() - rect.right()) <= tolerance;
    const bool top = qAbs(point.y() - rect.top()) <= tolerance;
    const bool bottom = qAbs(point.y() - rect.bottom()) <= tolerance;

    if (left && top)
        return CanvasHandle::TopLeft;
    if (right && top)
        return CanvasHandle::TopRight;
    if (left && bottom)
        return CanvasHandle::BottomLeft;
    if (right && bottom)
        return CanvasHandle::BottomRight;
    if (left)
        return CanvasHandle::Left;
    if (right)
        return CanvasHandle::Right;
    if (top)
        return CanvasHandle::Top;
    if (bottom)
        return CanvasHandle::Bottom;
    return CanvasHandle::None;
}

bool CanvasView::beginCanvasResize(const QPoint& viewportPos)
{
    const CanvasHandle handle = canvasHandleAt(viewportPos);
    if (handle == CanvasHandle::None)
        return false;

    m_canvasHandle = handle;
    m_resizingCanvas = true;
    m_resizeStartRect = m_scene->canvasRect();
    viewport()->setCursor(canvasHandleCursor(handle));
    return true;
}

QRectF CanvasView::canvasRectForDrag(const QPointF& scenePos) const
{
    QRectF rect = m_resizeStartRect;
    switch (m_canvasHandle) {
    case CanvasHandle::Left:
        rect.setLeft(scenePos.x());
        break;
    case CanvasHandle::Right:
        rect.setRight(scenePos.x());
        break;
    case CanvasHandle::Top:
        rect.setTop(scenePos.y());
        break;
    case CanvasHandle::Bottom:
        rect.setBottom(scenePos.y());
        break;
    case CanvasHandle::TopLeft:
        rect.setTopLeft(scenePos);
        break;
    case CanvasHandle::TopRight:
        rect.setTopRight(scenePos);
        break;
    case CanvasHandle::BottomLeft:
        rect.setBottomLeft(scenePos);
        break;
    case CanvasHandle::BottomRight:
        rect.setBottomRight(scenePos);
        break;
    case CanvasHandle::None:
        return rect;
    }

    rect = rect.normalized();
    constexpr qreal minimum = 200.0;
    if (rect.width() < minimum) {
        if (m_canvasHandle == CanvasHandle::Left || m_canvasHandle == CanvasHandle::TopLeft
            || m_canvasHandle == CanvasHandle::BottomLeft) {
            rect.setLeft(rect.right() - minimum);
        } else {
            rect.setRight(rect.left() + minimum);
        }
    }
    if (rect.height() < minimum) {
        if (m_canvasHandle == CanvasHandle::Top || m_canvasHandle == CanvasHandle::TopLeft
            || m_canvasHandle == CanvasHandle::TopRight) {
            rect.setTop(rect.bottom() - minimum);
        } else {
            rect.setBottom(rect.top() + minimum);
        }
    }
    return rect;
}

void CanvasView::updateCanvasResize(const QPointF& scenePos)
{
    if (!m_resizingCanvas)
        return;
    m_scene->applyCanvasRect(canvasRectForDrag(scenePos));
}

void CanvasView::endCanvasResize()
{
    if (!m_resizingCanvas)
        return;
    m_resizingCanvas = false;
    m_canvasHandle = CanvasHandle::None;
    m_scene->commitCanvasRect(m_resizeStartRect);
    const QSizeF size = m_scene->canvasSize();
    emit statusMessage(uiText(QStringLiteral("Canvas size: %1 × %2 (undoable)"))
                           .arg(qRound(size.width()))
                           .arg(qRound(size.height())));
    updateCursor();
}

void CanvasView::updateCanvasHoverCursor(const QPoint& viewportPos)
{
    if (m_panning || m_activeStroke || m_resizingCanvas)
        return;
    // 编辑模式按住 Ctrl 时左键用于平移整体视图，画板边缘不再提示可以拉伸。
    const CanvasHandle handle = ctrlPanActive() ? CanvasHandle::None : canvasHandleAt(viewportPos);
    if (handle == m_hoverHandle)
        return;
    m_hoverHandle = handle;
    if (handle == CanvasHandle::None)
        updateCursor();
    else
        viewport()->setCursor(canvasHandleCursor(handle));
}

void CanvasView::drawCanvasHandles(QPainter* painter)
{
    const QPolygon polygon = mapFromScene(m_scene->canvasRect());
    const QRectF rect = polygon.boundingRect();
    if (rect.width() < 30.0 || rect.height() < 30.0)
        return;

    const QPointF handles[8] = {
        rect.topLeft(),
        QPointF(rect.center().x(), rect.top()),
        rect.topRight(),
        QPointF(rect.right(), rect.center().y()),
        rect.bottomRight(),
        QPointF(rect.center().x(), rect.bottom()),
        rect.bottomLeft(),
        QPointF(rect.left(), rect.center().y())
    };

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(QColor(59, 130, 246), 1.2));
    painter->setBrush(QColor(255, 255, 255));
    for (const QPointF& point : handles) {
        const QRectF knob(point.x() - 4.5, point.y() - 4.5, 9.0, 9.0);
        painter->drawRoundedRect(knob, 2.0, 2.0);
    }
    painter->restore();
}
