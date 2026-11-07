#include "BoardView.h"

#include "BoardItem.h"
#include "BoardScene.h"
#include "StrokeItem.h"
#include "TextItem.h"

#include <QAction>
#include <QContextMenuEvent>
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
    m_panAnchorScene = mapToScene(position);
    m_panCenterScene = mapToScene(viewport()->rect().center());
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

    if (event->button() == Qt::LeftButton && m_mode == ToolMode::Brush && !m_activeStroke) {
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
        const QPointF delta = m_panAnchorScene - mapToScene(position);
        centerOn(m_panCenterScene + delta);
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
}

void BoardView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning) {
        endPan();
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
        QGraphicsItem* graphicsItem = itemAt(event->position().toPoint());
        auto* boardItem = qgraphicsitem_cast<BoardItem*>(graphicsItem);
        if (boardItem && !boardItem->isVisible()) {
            QGraphicsView::mouseDoubleClickEvent(event);
            return;
        }
        if (boardItem && !boardItem->isEditable()) {
            emit statusMessage(QStringLiteral("该图元所在图层未选中或已隐藏，请先在右侧图层面板选择对应图层。"));
            event->accept();
            return;
        }
        if (auto* textItem = dynamic_cast<TextItem*>(boardItem)) {
            emit editTextRequested(textItem);
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void BoardView::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_mode != ToolMode::Edit) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    const QPointF scenePosition = mapToScene(event->pos());
    BoardItem* item = nullptr;
    const QList<QGraphicsItem*> candidates = m_scene->items(scenePosition);
    for (QGraphicsItem* graphicsItem : candidates) {
        if (auto* boardItem = qgraphicsitem_cast<BoardItem*>(graphicsItem)) {
            if (!boardItem->isVisible())
                continue;
            item = boardItem;
            break;
        }
    }

    if (!item) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    if (!item->isEditable()) {
        emit statusMessage(QStringLiteral("该图元所在图层未选中或已隐藏，请先在右侧图层面板选择对应图层。"));
        event->accept();
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
        editText = menu.addAction(QStringLiteral("编辑文字…"));
        menu.addSeparator();
    }

    QAction* heightUp = menu.addAction(QStringLiteral("高度 +1（向上叠放）"));
    QAction* heightDown = menu.addAction(QStringLiteral("高度 -1（向下叠放）"));
    heightDown->setEnabled(item->height() > 0);
    menu.addSeparator();

    QAction* layerUp = menu.addAction(QStringLiteral("移到上一层图层"));
    QAction* layerDown = menu.addAction(QStringLiteral("移到下一层图层"));
    layerUp->setEnabled(layerIndex >= 0 && layerIndex < m_scene->layerCount() - 1);
    layerDown->setEnabled(layerIndex > 1);
    menu.addSeparator();

    QAction* remove = menu.addAction(QStringLiteral("删除图元"));
    remove->setEnabled(item->isSelected());

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;

    if (editText && chosen == editText)
        emit editTextRequested(textItem);
    else if (chosen == heightUp)
        m_scene->adjustItemHeight(item, 1);
    else if (chosen == heightDown)
        m_scene->adjustItemHeight(item, -1);
    else if (chosen == layerUp)
        m_scene->moveItemToAdjacentLayer(item, 1);
    else if (chosen == layerDown)
        m_scene->moveItemToAdjacentLayer(item, -1);
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
        emit statusMessage(QStringLiteral("已取消当前笔画"));
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

    m_scene->commitNewItem(stroke, QStringLiteral("添加笔画"));
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
