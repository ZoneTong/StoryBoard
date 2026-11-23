#include "canvas/CanvasView.h"

#include "app/I18n.h"
#include "canvas/CanvasViewCursor.h"
#include "canvas/CanvasViewHitTester.h"
#include "canvas/controllers/CanvasContextMenuController.h"
#include "canvas/controllers/CanvasPanController.h"
#include "canvas/controllers/CanvasResizeController.h"
#include "canvas/controllers/CanvasStrokeController.h"
#include "canvas/controllers/CanvasZoomController.h"
#include "items/CanvasItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/shapes/ShapeAbstractItem.h"
#include "scene/CanvasScene.h"

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

namespace {

// 零面积描线图元：曲线，以及直线等描线类图形。
bool IsStrokeItem(CanvasItem* item)
{
    if (dynamic_cast<LineAbstractItem*>(item)) {
        return true;
    }
    // 直线 / 弧线 / 正弦已改为曲线图元，形状类里不再有零面积图元。
    return false;
}

// 需求 23：描线图元的 shape() 含命中容差外扩。若“实际笔迹”（paintedShape）没盖住
// 点击点、只是靠容差擦边命中，点击意图更可能是下方被横穿的填充图形。
bool IsStrokeHitToleranceOnly(CanvasItem* item, const QPointF& scenePos)
{
    if (!IsStrokeItem(item)) {
        return false;
    }
    return !item->PaintedShape().contains(item->mapFromScene(scenePos));
}

} // namespace

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
    m_groupBox = std::make_unique<CanvasGroupBoxController>(this, scene);

    // 分组失败原因由场景写出，视图层转成状态栏文案（§7.8）。
    connect(scene, &CanvasScene::groupOperationFailed, this,
            [this](GroupFailureReason reason) { emit statusMessage(GroupFailureReasonText(reason)); });
    // 场景层的规则拒绝（组内成员单独换层）走同一路状态栏文案，由 MainWindow 统一限时显示。
    connect(scene, &CanvasScene::statusMessage, this, [this](const QString& message) { emit statusMessage(message); });

    UpdateCursor();
}

qreal CanvasView::Zoom() const
{
    return m_zoom->Zoom();
}

void CanvasView::SetMode(ToolMode mode)
{
    if (m_mode == mode) {
        return;
    }

    FinishCurrentStroke();
    m_mode = mode;
    m_scene->SetMode(mode);

    switch (mode) {
    case ToolMode::VIEW:
        setDragMode(QGraphicsView::NoDrag);
        m_scene->clearSelection();
        FitAll();
        break;
    case ToolMode::BRUSH:
        setDragMode(QGraphicsView::NoDrag);
        m_scene->clearSelection();
        break;
    case ToolMode::EDIT:
        setDragMode(QGraphicsView::RubberBandDrag);
        break;
    case ToolMode::TEXT:
        setDragMode(QGraphicsView::NoDrag);
        m_scene->clearSelection();
        break;
    }

    UpdateCursor();
    emit modeChanged(mode);
}

void CanvasView::FitAll()
{
    m_zoom->FitAll();
}

void CanvasView::ZoomIn()
{
    m_zoom->ZoomIn();
}

void CanvasView::ZoomOut()
{
    m_zoom->ZoomOut();
}

void CanvasView::ActualSize()
{
    m_zoom->ActualSize();
}

void CanvasView::SetBrushColor(const QColor& color)
{
    m_stroke->SetBrushColor(color);
}

void CanvasView::SetBrushWidth(qreal width)
{
    m_stroke->SetBrushWidth(width);
}

void CanvasView::SetStrokeSmoothing(qreal tolerance)
{
    m_stroke->SetStrokeSmoothing(tolerance);
}

void CanvasView::AddContextMenuGroup(const QString& title, const QList<QAction*>& actions)
{
    m_menu->AddGroup(title, actions);
}

void CanvasView::SetCanvasColor(const QColor& color)
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

void CanvasView::UpdateCursor()
{
    m_cursor->Refresh(m_mode, m_pan->IsPanning());
}

void CanvasView::RefreshCursor()
{
    UpdateCursor();
}

void CanvasView::ShowDefaultContextMenu(QContextMenuEvent* event)
{
    BaseContextMenuEvent(event);
}

void CanvasView::ForwardWheelEvent(QWheelEvent* event)
{
    QGraphicsView::wheelEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
    m_zoom->HandleWheel(event);
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    // 右键点中已选图元时不要走默认选择逻辑：QGraphicsView 会先把多选收成单选，
    // 导致随后的右键菜单拿不到完整选择，“组合选中图元”被误禁用。
    if (m_mode == ToolMode::EDIT && event->button() == Qt::RightButton) {
        CanvasItem* item = m_hits->EditableItemAt(position);
        if (item && item->isSelected()) {
            event->accept();
            return;
        }
    }

    // 平移只保留中键与查看模式左键：编辑模式不再用 Ctrl+左键平移，把 Ctrl 留给多选。
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && (m_mode == ToolMode::VIEW || m_spaceDown))) {
        m_pan->BeginPan(position);
        event->accept();
        return;
    }

    // 组选框手柄优先于图元自身手柄：完整整组选中时由组框统一缩放 / 旋转（§7.6）。
    if (m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
        const int groupHandle = m_groupBox->HitTest(position);
        if (groupHandle != static_cast<int>(GroupBoxHandle::NONE) &&
            groupHandle != static_cast<int>(GroupBoxHandle::BODY)) {
            m_groupBox->BeginDrag(position, groupHandle);
            event->accept();
            return;
        }
    }

    if (m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton && m_resize->BeginResize(position)) {
        event->accept();
        return;
    }

    // 需求 23：点击落在填充图形内部时，最上层描线图元（曲线/直线）若只是靠命中
    // 容差“擦边”够到点击点、笔迹本体没压住该点，就让下方填充图形优先接收本次
    // 点击，避免点矩形内部却选中了横穿矩形的曲线。Ctrl 加选与已选中图元不受影响
    // （后者点击的可能是它的控制手柄）。
    if (m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
        const QPointF scenePos = mapToScene(position);
        const QList<QGraphicsItem*> clickCandidates = m_scene->items(scenePos);
        CanvasItem* strokeTop = nullptr;
        for (QGraphicsItem* graphicsItem : clickCandidates) {
            auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
            if (!canvasItem || !canvasItem->isVisible() || !m_scene->IsItemEditable(canvasItem)) {
                continue;
            }
            strokeTop = canvasItem;
            break;
        }
        if (strokeTop && !strokeTop->isSelected() && IsStrokeHitToleranceOnly(strokeTop, scenePos)) {
            CanvasItem* fillBelow = nullptr;
            for (QGraphicsItem* graphicsItem : clickCandidates) {
                auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
                if (canvasItem == strokeTop || !canvasItem || !canvasItem->isVisible() ||
                    !m_scene->IsItemEditable(canvasItem) || IsStrokeItem(canvasItem)) {
                    continue;
                }
                if (canvasItem->shape().contains(canvasItem->mapFromScene(scenePos))) {
                    fillBelow = canvasItem;
                    break;
                }
            }
            if (fillBelow) {
                const Qt::MouseButtons savedButtons = strokeTop->acceptedMouseButtons();
                const bool savedSelectable = strokeTop->flags().testFlag(QGraphicsItem::ItemIsSelectable);
                strokeTop->setFlag(QGraphicsItem::ItemIsSelectable, false);
                strokeTop->setAcceptedMouseButtons(Qt::NoButton);
                QGraphicsView::mousePressEvent(event);
                strokeTop->setAcceptedMouseButtons(savedButtons);
                strokeTop->setFlag(QGraphicsItem::ItemIsSelectable, savedSelectable);
                return;
            }
        }
    }

    // 需求 15：重叠图元优先命中图层面板里已选图层中的图元。
    // QGraphicsScene 的默认选择总是选最上层，这里在按下期间临时让“非优先图层”
    // 的图元不可选中，交给基类完成选择/拖拽后再恢复，避免改变真实可见顺序。
    if (m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton) {
        CanvasItem* preferred = m_hits->PreferredEditableItemAt(position);
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
                if (!m_scene->IsItemEditable(canvasItem)) {
                    continue;
                }
                if (m_scene->PreferredLayerIds().contains(canvasItem->LayerId())) {
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

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::BRUSH && !m_stroke->HasActiveStroke()) {
        if (!m_stroke->BeginStroke(position)) {
            event->accept();
            return;
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::TEXT) {
        if (!m_scene->IsActiveLayerWritable()) {
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
    if (m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
        CanvasItem* item = m_hits->EditableItemAt(position);
        if (item && item->GroupId() > 0) {
            m_scene->SelectGroupOfItem(item);
        }
    }

    // 空白处按下即视为一次可能的框选起点：交给基类开橡皮筋，松手时再补判定。
    m_bandCandidate = m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton &&
                      !event->modifiers().testFlag(Qt::ControlModifier) && !m_hits->EditableItemAt(position);
    if (m_bandCandidate) {
        m_bandOrigin = position;
    }

    QGraphicsView::mousePressEvent(event);

    // 基类按下时可能把多选收成单选，按完再补一次整组选择。
    if (m_mode == ToolMode::EDIT && event->button() == Qt::LeftButton &&
        !event->modifiers().testFlag(Qt::ControlModifier)) {
        CanvasItem* item = m_hits->EditableItemAt(position);
        if (item && item->GroupId() > 0 && m_scene->SelectedCanvasItems().size() > 1) {
            m_scene->SelectGroupOfItem(item);
        }
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();

    if (m_pan->MovePan(position)) {
        event->accept();
        return;
    }

    if (m_groupBox->IsDragging()) {
        m_groupBox->UpdateDrag(position, event->modifiers());
        event->accept();
        return;
    }

    if (m_resize->IsResizing()) {
        m_resize->UpdateResize(mapToScene(position));
        event->accept();
        return;
    }

    if (m_stroke->HasActiveStroke()) {
        m_stroke->AppendPoint(position);
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
    m_resize->UpdateHoverCursor(position, m_pan->IsPanning(), m_stroke->HasActiveStroke());
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pan->IsPanning()) {
        m_pan->EndPan();
        event->accept();
        return;
    }

    if (m_groupBox->IsDragging() && event->button() == Qt::LeftButton) {
        m_groupBox->EndDrag();
        event->accept();
        return;
    }

    if (m_resize->IsResizing() && event->button() == Qt::LeftButton) {
        m_resize->EndResize();
        event->accept();
        return;
    }

    if (m_stroke->HasActiveStroke() && event->button() == Qt::LeftButton) {
        FinishCurrentStroke();
        event->accept();
        return;
    }

    // 基类先按 Qt 的口径（shape()，含描边外扩与手柄）完成一次框选，并把落选图元取消选中；
    // 补漏必须放在它之后，否则刚补选中的图元会被基类的 ReplaceSelection 再清掉。
    const QPoint bandEnd = event->position().toPoint();
    const bool applyBandFix = m_bandCandidate && event->button() == Qt::LeftButton;
    m_bandCandidate = false;

    QGraphicsView::mouseReleaseEvent(event);

    if (applyBandFix) {
        SelectItemsInsideBand(m_bandOrigin, bandEnd);
    }
}

void CanvasView::SelectItemsInsideBand(const QPoint& from, const QPoint& to)
{
    const QRect viewRect = QRect(from, to).normalized();
    if (viewRect.width() < 3 && viewRect.height() < 3) {
        return;
    }
    // Qt 的框选判定用 shape()，含描边外扩与手柄；框线贴着图形时会被判成“没框全”。
    // 这里按图元实际外框（frameRect）再判一次“整框包含”，作为补漏。
    const QRectF sceneRect = mapToScene(viewRect).boundingRect();
    for (QGraphicsItem* graphicsItem : m_scene->items()) {
        auto* item = qgraphicsitem_cast<CanvasItem*>(graphicsItem);
        if (!item || !item->isVisible() || !item->IsEditable()) {
            continue;
        }
        if (!item->flags().testFlag(QGraphicsItem::ItemIsSelectable)) {
            continue;
        }
        const QRectF itemRect = item->mapRectToScene(item->FrameRect());
        if (sceneRect.contains(itemRect)) {
            item->setSelected(true);
        }
    }
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // 需求 42：属性窗在查看/画笔/文字模式下也能打开（只读）；
        // 编辑模式仍要求图元可编辑，锁定图层维持原来的“先解锁”提示。
        if (m_mode == ToolMode::EDIT) {
            CanvasItem* canvasItem = m_hits->EditableItemAt(event->position().toPoint());
            if (!canvasItem) {
                if (m_hits->TopItemAt(event->position().toPoint())) {
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

        CanvasItem* canvasItem = m_hits->PropertyItemAt(event->position().toPoint());
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
    m_menu->ShowMenu(event);
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        if (!m_pan->IsPanning()) {
            viewport()->setCursor(Qt::OpenHandCursor);
        }
        event->accept();
        return;
    }

    // 需求 7：画笔模式下按 Esc 退出回编辑模式；正在画的笔画先取消，不落库。
    if (event->key() == Qt::Key_Escape && m_mode == ToolMode::BRUSH) {
        if (m_stroke->HasActiveStroke()) {
            m_stroke->CancelStroke();
        }
        SetMode(ToolMode::EDIT);
        emit statusMessage(tr("Brush mode exited; back to edit mode"));
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && m_stroke->HasActiveStroke()) {
        m_stroke->CancelStroke();
        emit statusMessage(tr("Current stroke canceled"));
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        scene()->clearSelection();
        event->accept();
        return;
    }

    if (m_mode == ToolMode::EDIT) {
        const qreal step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
        QPointF delta;
        switch (event->key()) {
        case Qt::Key_Left:
            delta.rx() -= step;
            break;
        case Qt::Key_Right:
            delta.rx() += step;
            break;
        case Qt::Key_Up:
            delta.ry() -= step;
            break;
        case Qt::Key_Down:
            delta.ry() += step;
            break;
        default:
            break;
        }
        if (!delta.isNull()) {
            m_scene->NudgeSelected(delta);
            event->accept();
            return;
        }
    }

    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        UpdateCursor();
        event->accept();
        return;
    }
    if (!m_pan->IsPanning()) {
        m_spaceDown = false;
        UpdateCursor();
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::focusOutEvent(QFocusEvent* event)
{
    // 焦点离开后收不到按键抬起事件，按住状态必须复位，否则回来后 Ctrl / 空格会被误判为一直按住。
    if (!m_pan->IsPanning()) {
        m_spaceDown = false;
        UpdateCursor();
    }
    QGraphicsView::focusOutEvent(event);
}

void CanvasView::FinishCurrentStroke()
{
    m_stroke->FinishStroke();
}

void CanvasView::CancelCurrentStroke()
{
    m_stroke->CancelStroke();
}

void CanvasView::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
    if (m_mode != ToolMode::EDIT) {
        return;
    }
    QPainter painter(viewport());
    m_resize->DrawHandles(&painter);
    m_groupBox->DrawOverlay(&painter);
}

void CanvasView::leaveEvent(QEvent* event)
{
    m_resize->ResetHoverOnLeave();
    QGraphicsView::leaveEvent(event);
}
