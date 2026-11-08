#include "I18n.h"

#include "BoardView.h"

#include "BoardItem.h"
#include "BoardScene.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QCursor>
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

BoardView::BoardView(BoardScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                   | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, false);
    // QGraphicsView 默认直接用 QGraphicsScenePrivate::drawItems 画图元，不会调用
    // BoardScene::drawItems 这个虚函数；开启 IndirectPainting 后才会走
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
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumSize(360, 260);
    updateCursor();
}

qreal BoardView::zoom() const
{
    return transform().m11();
}

void BoardView::setMode(ToolMode mode)
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

void BoardView::fitAll()
{
    if (viewport()->width() <= 8 || viewport()->height() <= 8)
        return;

    QRectF target = m_scene->contentRect();
    if (target.isEmpty())
        target = m_scene->boardRect();

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

void BoardView::zoomIn()
{
    applyScale(1.22);
}

void BoardView::zoomOut()
{
    applyScale(1.0 / 1.22);
}

void BoardView::actualSize()
{
    const QPointF center = mapToScene(viewport()->rect().center());
    setTransformationAnchor(QGraphicsView::NoAnchor);
    resetTransform();
    centerOn(center);
    emitZoom();
}

void BoardView::applyScale(qreal factor)
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

void BoardView::emitZoom()
{
    emit zoomChanged(zoom());
}

void BoardView::setBrushColor(const QColor& color)
{
    if (color.isValid())
        m_brushColor = color;
}

void BoardView::setBrushWidth(qreal width)
{
    m_brushWidth = qBound<qreal>(0.5, width, 64.0);
}

void BoardView::setCanvasColor(const QColor& color)
{
    if (!color.isValid())
        return;
    m_canvasColor = color;
    viewport()->update();
}

void BoardView::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, m_canvasColor);
    QGraphicsView::drawBackground(painter, rect);
}

void BoardView::updateCursor()
{
    if (m_panning) {
        viewport()->setCursor(Qt::ClosedHandCursor);
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

void BoardView::beginPan(const QPoint& position)
{
    m_panning = true;
    m_panStart = position;
    m_panCenterScene = mapToScene(viewport()->rect()).boundingRect().center();
    viewport()->setCursor(Qt::ClosedHandCursor);
}

void BoardView::endPan()
{
    m_panning = false;
    updateCursor();
}

void BoardView::wheelEvent(QWheelEvent* event)
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

void BoardView::mousePressEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && (m_spaceDown || m_mode == ToolMode::View))) {
        beginPan(position);
        event->accept();
        return;
    }

    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton
        && beginBoardResize(position)) {
        event->accept();
        return;
    }

    // 需求 15：重叠图元优先命中图层面板里已选图层中的图元。
    // QGraphicsScene 的默认选择总是选最上层，这里在按下期间临时让“非优先图层”
    // 的图元不可选中，交给基类完成选择/拖拽后再恢复，避免改变真实可见顺序。
    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton) {
        BoardItem* preferred = preferredEditableItemAt(position);
        const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(position));
        const bool preferredIsTop = !candidates.isEmpty()
            && qgraphicsitem_cast<BoardItem*>(candidates.first()) == preferred;
        if (preferred && !preferredIsTop) {
            struct SuspendedItem {
                BoardItem* item;
                Qt::MouseButtons buttons;
                bool selectable;
            };
            QList<SuspendedItem> suspended;
            for (QGraphicsItem* graphicsItem : candidates) {
                auto* boardItem = qgraphicsitem_cast<BoardItem*>(graphicsItem);
                if (!boardItem || boardItem == preferred || !boardItem->isVisible())
                    continue;
                if (!m_scene->isItemEditable(boardItem))
                    continue;
                if (m_scene->preferredLayerIds().contains(boardItem->layerId()))
                    continue;
                suspended.append({ boardItem,
                                   boardItem->acceptedMouseButtons(),
                                   boardItem->flags() & QGraphicsItem::ItemIsSelectable });
                boardItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
                boardItem->setAcceptedMouseButtons(Qt::NoButton);
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
            emit statusMessage(QStringLiteral(
                "当前图层已锁定或已隐藏，无法在该图层绘制。请先在右侧图层面板解锁或切换图层。"));
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
            emit statusMessage(QStringLiteral(
                "当前图层已锁定或已隐藏，无法在该图层放置文字。请先在右侧图层面板解锁或切换图层。"));
            event->accept();
            return;
        }
        emit textPlacementRequested(mapToScene(position));
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void BoardView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();

    if (m_panning) {
        // 用“按下点 → 当前点”的场景位移来平移：两个点在同一时刻的变换下相减，
        // 结果与当前视图位置无关。若拿按下时的场景锚点去和当前 mapToScene 相减，
        // 视图每移动一次都会把上次的位移重复计入，画面就会来回抖动。
        const QPointF sceneDelta = mapToScene(position) - mapToScene(m_panStart);
        centerOn(m_panCenterScene - sceneDelta);
        event->accept();
        return;
    }

    if (m_resizingBoard) {
        updateBoardResize(mapToScene(position));
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
    updateBoardHoverCursor(position);
}

void BoardView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning) {
        endPan();
        event->accept();
        return;
    }

    if (m_resizingBoard && event->button() == Qt::LeftButton) {
        endBoardResize();
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

void BoardView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_mode == ToolMode::Edit && event->button() == Qt::LeftButton) {
        BoardItem* boardItem = editableItemAt(event->position().toPoint());
        if (!boardItem) {
            if (topBoardItemAt(event->position().toPoint())) {
                emit statusMessage(QStringLiteral(
                    "该图元所在图层已锁定或已隐藏，无法编辑。请先在右侧图层面板解锁或显示该图层。"));
                event->accept();
                return;
            }
            QGraphicsView::mouseDoubleClickEvent(event);
            return;
        }
        // 双击图元统一弹出属性窗；文字属性的“编辑文字…”按钮再进入文字编辑。
        emit itemPropertiesRequested(boardItem);
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void BoardView::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_mode != ToolMode::Edit) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    BoardItem* item = editableItemAt(event->pos());
    if (!item) {
        if (topBoardItemAt(event->pos())) {
            emit statusMessage(QStringLiteral(
                "该图元所在图层已锁定或已隐藏，无法编辑。请先在右侧图层面板解锁或显示该图层。"));
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

    const BoardLayer itemLayer = m_scene->layer(item->layerId());
    const int layerIndex = m_scene->layerIndex(item->layerId());

    QMenu menu(this);
    QAction* title = menu.addAction(
        QStringLiteral("%1 · %2 · height %3")
            .arg(item->displayName(), itemLayer.name)
            .arg(item->height()));
    title->setEnabled(false);
    menu.addSeparator();

    TextItem* textItem = dynamic_cast<TextItem*>(item);
    QAction* editText = nullptr;
    if (textItem) {
        editText = menu.addAction(uiText(QStringLiteral("Edit text…")));
        menu.addSeparator();
    }

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

    if (editText && chosen == editText)
        emit editTextRequested(textItem);
    else if (chosen == heightUp)
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

void BoardView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        if (!m_panning)
            viewport()->setCursor(Qt::OpenHandCursor);
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

void BoardView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        updateCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void BoardView::finishCurrentStroke()
{
    if (!m_activeStroke)
        return;

    StrokeItem* stroke = m_activeStroke;
    m_activeStroke = nullptr;
    stroke->endStroke();

    if (stroke->isEmpty()) {
        m_scene->removeItem(stroke);
        delete stroke;
        updateCursor();
        return;
    }

    m_scene->commitNewItem(stroke, uiText(QStringLiteral("Add stroke")));
    updateCursor();
}

void BoardView::cancelCurrentStroke()
{
    if (!m_activeStroke)
        return;

    StrokeItem* stroke = m_activeStroke;
    m_activeStroke = nullptr;
    m_scene->removeItem(stroke);
    delete stroke;
    updateCursor();
}

void BoardView::paintEvent(QPaintEvent* event)
{
    QGraphicsView::paintEvent(event);
    if (m_mode != ToolMode::Edit)
        return;
    QPainter painter(viewport());
    drawBoardHandles(&painter);
}

void BoardView::leaveEvent(QEvent* event)
{
    if (!m_resizingBoard) {
        m_hoverHandle = BoardHandle::None;
        updateCursor();
    }
    QGraphicsView::leaveEvent(event);
}

BoardItem* BoardView::topBoardItemAt(const QPoint& viewportPos) const
{
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* boardItem = qgraphicsitem_cast<BoardItem*>(graphicsItem);
        if (boardItem && boardItem->isVisible())
            return boardItem;
    }
    return nullptr;
}

BoardItem* BoardView::preferredEditableItemAt(const QPoint& viewportPos) const
{
    const QList<int> preferred = m_scene->preferredLayerIds();
    if (preferred.isEmpty())
        return nullptr;

    // 保持顶层优先：列表中越靠前叠得越高。
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* boardItem = qgraphicsitem_cast<BoardItem*>(graphicsItem);
        if (!boardItem || !boardItem->isVisible())
            continue;
        if (preferred.contains(boardItem->layerId()) && m_scene->isItemEditable(boardItem))
            return boardItem;
    }
    return nullptr;
}

BoardItem* BoardView::editableItemAt(const QPoint& viewportPos) const
{
    // 命中要“穿透”锁定或隐藏的图层：自上而下找第一个真正可编辑的图元，
    // 这样锁定的顶层图元不会挡住下面未锁定图层上的图元。
    const QList<QGraphicsItem*> candidates = m_scene->items(mapToScene(viewportPos));
    const QList<int> preferred = m_scene->preferredLayerIds();
    BoardItem* fallback = nullptr;
    for (QGraphicsItem* graphicsItem : candidates) {
        auto* boardItem = qgraphicsitem_cast<BoardItem*>(graphicsItem);
        if (!boardItem || !boardItem->isVisible())
            continue;
        if (!m_scene->isItemEditable(boardItem))
            continue;
        // 重叠区域优先命中图层面板中已选图层里的图元，其余仍按叠放顺序取最上层。
        if (preferred.contains(boardItem->layerId()))
            return boardItem;
        if (!fallback)
            fallback = boardItem;
    }
    return fallback;
}

Qt::CursorShape BoardView::boardHandleCursor(BoardHandle handle)
{
    switch (handle) {
    case BoardHandle::Left:
    case BoardHandle::Right:
        return Qt::SizeHorCursor;
    case BoardHandle::Top:
    case BoardHandle::Bottom:
        return Qt::SizeVerCursor;
    case BoardHandle::TopLeft:
    case BoardHandle::BottomRight:
        return Qt::SizeFDiagCursor;
    case BoardHandle::TopRight:
    case BoardHandle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case BoardHandle::None:
        break;
    }
    return Qt::ArrowCursor;
}

BoardView::BoardHandle BoardView::boardHandleAt(const QPoint& viewportPos) const
{
    if (m_mode != ToolMode::Edit)
        return BoardHandle::None;

    const QRectF rect = mapFromScene(m_scene->boardRect()).boundingRect();
    const QPointF point = viewportPos;
    const qreal tolerance = 7.0;
    const QRectF outer = rect.adjusted(-tolerance, -tolerance, tolerance, tolerance);
    if (!outer.contains(point))
        return BoardHandle::None;

    const bool left = qAbs(point.x() - rect.left()) <= tolerance;
    const bool right = qAbs(point.x() - rect.right()) <= tolerance;
    const bool top = qAbs(point.y() - rect.top()) <= tolerance;
    const bool bottom = qAbs(point.y() - rect.bottom()) <= tolerance;

    if (left && top)
        return BoardHandle::TopLeft;
    if (right && top)
        return BoardHandle::TopRight;
    if (left && bottom)
        return BoardHandle::BottomLeft;
    if (right && bottom)
        return BoardHandle::BottomRight;
    if (left)
        return BoardHandle::Left;
    if (right)
        return BoardHandle::Right;
    if (top)
        return BoardHandle::Top;
    if (bottom)
        return BoardHandle::Bottom;
    return BoardHandle::None;
}

bool BoardView::beginBoardResize(const QPoint& viewportPos)
{
    const BoardHandle handle = boardHandleAt(viewportPos);
    if (handle == BoardHandle::None)
        return false;

    m_boardHandle = handle;
    m_resizingBoard = true;
    m_resizeStartRect = m_scene->boardRect();
    viewport()->setCursor(boardHandleCursor(handle));
    return true;
}

QRectF BoardView::boardRectForDrag(const QPointF& scenePos) const
{
    QRectF rect = m_resizeStartRect;
    switch (m_boardHandle) {
    case BoardHandle::Left:
        rect.setLeft(scenePos.x());
        break;
    case BoardHandle::Right:
        rect.setRight(scenePos.x());
        break;
    case BoardHandle::Top:
        rect.setTop(scenePos.y());
        break;
    case BoardHandle::Bottom:
        rect.setBottom(scenePos.y());
        break;
    case BoardHandle::TopLeft:
        rect.setTopLeft(scenePos);
        break;
    case BoardHandle::TopRight:
        rect.setTopRight(scenePos);
        break;
    case BoardHandle::BottomLeft:
        rect.setBottomLeft(scenePos);
        break;
    case BoardHandle::BottomRight:
        rect.setBottomRight(scenePos);
        break;
    case BoardHandle::None:
        return rect;
    }

    rect = rect.normalized();
    constexpr qreal minimum = 200.0;
    if (rect.width() < minimum) {
        if (m_boardHandle == BoardHandle::Left || m_boardHandle == BoardHandle::TopLeft
            || m_boardHandle == BoardHandle::BottomLeft) {
            rect.setLeft(rect.right() - minimum);
        } else {
            rect.setRight(rect.left() + minimum);
        }
    }
    if (rect.height() < minimum) {
        if (m_boardHandle == BoardHandle::Top || m_boardHandle == BoardHandle::TopLeft
            || m_boardHandle == BoardHandle::TopRight) {
            rect.setTop(rect.bottom() - minimum);
        } else {
            rect.setBottom(rect.top() + minimum);
        }
    }
    return rect;
}

void BoardView::updateBoardResize(const QPointF& scenePos)
{
    if (!m_resizingBoard)
        return;
    m_scene->applyBoardRect(boardRectForDrag(scenePos));
}

void BoardView::endBoardResize()
{
    if (!m_resizingBoard)
        return;
    m_resizingBoard = false;
    m_boardHandle = BoardHandle::None;
    m_scene->commitBoardRect(m_resizeStartRect);
    const QSizeF size = m_scene->boardSize();
    emit statusMessage(uiText(QStringLiteral("Board size: %1 × %2 (undoable)"))
                           .arg(qRound(size.width()))
                           .arg(qRound(size.height())));
    updateCursor();
}

void BoardView::updateBoardHoverCursor(const QPoint& viewportPos)
{
    if (m_panning || m_activeStroke || m_resizingBoard)
        return;
    const BoardHandle handle = boardHandleAt(viewportPos);
    if (handle == m_hoverHandle)
        return;
    m_hoverHandle = handle;
    if (handle == BoardHandle::None)
        updateCursor();
    else
        viewport()->setCursor(boardHandleCursor(handle));
}

void BoardView::drawBoardHandles(QPainter* painter)
{
    const QPolygon polygon = mapFromScene(m_scene->boardRect());
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
