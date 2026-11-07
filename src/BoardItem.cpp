#include "BoardItem.h"

#include "BoardScene.h"

#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QJsonArray>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

#include <utility>

namespace {
constexpr qreal kMinimumSide = 8.0;
constexpr qreal kHandleHitPadding = 2.0;

bool isLeftHandle(BoardItem::Handle handle)
{
    return handle == BoardItem::TopLeft || handle == BoardItem::Left || handle == BoardItem::BottomLeft;
}

bool isRightHandle(BoardItem::Handle handle)
{
    return handle == BoardItem::TopRight || handle == BoardItem::Right || handle == BoardItem::BottomRight;
}

bool isTopHandle(BoardItem::Handle handle)
{
    return handle == BoardItem::TopLeft || handle == BoardItem::Top || handle == BoardItem::TopRight;
}

bool isBottomHandle(BoardItem::Handle handle)
{
    return handle == BoardItem::BottomLeft || handle == BoardItem::Bottom || handle == BoardItem::BottomRight;
}

bool isCornerHandle(BoardItem::Handle handle)
{
    return handle == BoardItem::TopLeft || handle == BoardItem::TopRight
        || handle == BoardItem::BottomLeft || handle == BoardItem::BottomRight;
}

} // namespace

BoardItem::BoardItem(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, false);
    setAcceptHoverEvents(false);
    setCursor(Qt::ArrowCursor);
}

QSizeF BoardItem::minimumSize()
{
    return QSizeF(kMinimumSide, kMinimumSide);
}

QRectF BoardItem::contentRect() const
{
    return QRectF(0.0, 0.0, m_size.width(), m_size.height());
}

void BoardItem::setContentSize(const QSizeF& size)
{
    const QSizeF clamped(qMax(kMinimumSide, size.width()), qMax(kMinimumSide, size.height()));
    if (qFuzzyCompare(clamped.width(), m_size.width())
        && qFuzzyCompare(clamped.height(), m_size.height())) {
        return;
    }
    prepareGeometryChange();
    m_size = clamped;
    update();
    contentResized();
}

void BoardItem::setLayerId(int layerId)
{
    m_layerId = qMax(0, layerId);
}

void BoardItem::setHeight(int height)
{
    m_height = qMax(0, height);
}

QRectF BoardItem::sceneContentRect() const
{
    return mapRectToScene(contentRect());
}

QRectF BoardItem::boundingRect() const
{
    const qreal padding = framePadding();
    return contentRect().adjusted(-padding, -padding, padding, padding);
}

QPainterPath BoardItem::shape() const
{
    QPainterPath path;
    path.addRect(contentRect());
    path.addPath(selectionHandleShape());
    return path;
}

QPainterPath BoardItem::outlineShape() const
{
    QPainterPath path;
    path.addRect(contentRect());
    return path;
}

QPainterPath BoardItem::paintedShape() const
{
    // 基类按内容矩形整体涂满处理：图片与文字等子类会覆写这里。
    return outlineShape();
}

QPainterPath BoardItem::selectionHandleShape() const
{
    QPainterPath path;
    if (!isSelected() || !isEditable() || !hasSingleSelection())
        return path;

    // Selection handles are centred on the content corners, so half of every
    // handle lies outside the item geometry. Drawing them without adding them
    // to shape() leaves the visible handle unclickable; for outlines that never
    // reach the corner (ellipse, triangle, diamond, star) the handle could not
    // be grabbed at all.
    for (int handle = TopLeft; handle <= Left; ++handle) {
        path.addRect(handleRect(static_cast<Handle>(handle))
                         .adjusted(-kHandleHitPadding, -kHandleHitPadding,
                                   kHandleHitPadding, kHandleHitPadding));
    }
    return path;
}

void BoardItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    paintContent(painter);
    painter->restore();

    if (isSelected() && !isExporting()) {
        painter->save();
        drawSelectionOverlay(painter);
        painter->restore();
    }
}

bool BoardItem::isEditable() const
{
    const BoardScene* board = boardScene();
    return board && board->isItemEditable(this);
}

bool BoardItem::isExporting() const
{
    const BoardScene* board = boardScene();
    return board && board->isExporting();
}

BoardScene* BoardItem::boardScene() const
{
    return dynamic_cast<BoardScene*>(scene());
}

qreal BoardItem::viewScale() const
{
    if (scene() && !scene()->views().isEmpty()) {
        const qreal scale = scene()->views().constFirst()->transform().m11();
        return qMax<qreal>(0.0001, scale);
    }
    return 1.0;
}

bool BoardItem::hasSingleSelection() const
{
    return scene() && isSelected() && scene()->selectedItems().size() == 1;
}

qreal BoardItem::handleSize() const
{
    return qBound<qreal>(6.0, 9.0 / viewScale(), 40.0);
}

QPointF BoardItem::handleCenter(Handle handle) const
{
    const QRectF rect = contentRect();
    switch (handle) {
    case TopLeft:
        return rect.topLeft();
    case Top:
        return QPointF(rect.center().x(), rect.top());
    case TopRight:
        return rect.topRight();
    case Right:
        return QPointF(rect.right(), rect.center().y());
    case BottomRight:
        return rect.bottomRight();
    case Bottom:
        return QPointF(rect.center().x(), rect.bottom());
    case BottomLeft:
        return rect.bottomLeft();
    case Left:
        return QPointF(rect.left(), rect.center().y());
    case NoHandle:
        break;
    }
    return rect.center();
}

QRectF BoardItem::handleRect(Handle handle) const
{
    const qreal size = handleSize();
    const QPointF center = handleCenter(handle);
    return QRectF(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
}

BoardItem::Handle BoardItem::handleAt(const QPointF& pos) const
{
    const qreal hitPadding = qMin<qreal>(10.0 / viewScale(), 14.0);
    for (int handle = TopLeft; handle <= Left; ++handle) {
        const QRectF rect = handleRect(static_cast<Handle>(handle)).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (rect.contains(pos))
            return static_cast<Handle>(handle);
    }
    return NoHandle;
}

QCursor BoardItem::cursorForHandle(Handle handle) const
{
    switch (handle) {
    case TopLeft:
    case BottomRight:
        return QCursor(Qt::SizeFDiagCursor);
    case TopRight:
    case BottomLeft:
        return QCursor(Qt::SizeBDiagCursor);
    case Top:
    case Bottom:
        return QCursor(Qt::SizeVerCursor);
    case Left:
    case Right:
        return QCursor(Qt::SizeHorCursor);
    case NoHandle:
        break;
    }
    return QCursor(Qt::ArrowCursor);
}

void BoardItem::drawSelectionOverlay(QPainter* painter)
{
    QPen borderPen(QColor(79, 124, 255));
    borderPen.setWidthF(1.3);
    borderPen.setCosmetic(true);
    borderPen.setStyle(Qt::DashLine);
    borderPen.setDashPattern({ 5.0, 4.0 });
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(contentRect());

    if (!isEditable() || !hasSingleSelection())
        return;

    QPen handlePen(QColor(79, 124, 255));
    handlePen.setWidthF(1.4);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int handle = TopLeft; handle <= Left; ++handle) {
        painter->drawRect(handleRect(static_cast<Handle>(handle)));
    }
}

void BoardItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if (!isEditable()) {
        unsetCursor();
        QGraphicsObject::hoverMoveEvent(event);
        return;
    }
    updateHoverCursor(event->pos());
    QGraphicsObject::hoverMoveEvent(event);
}

void BoardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void BoardItem::updateHoverCursor(const QPointF& pos)
{
    if (!isEditable()) {
        unsetCursor();
        return;
    }
    if (hasSingleSelection()) {
        const Handle handle = handleAt(pos);
        if (handle != NoHandle) {
            setCursor(cursorForHandle(handle));
            return;
        }
    }
    setCursor(QCursor(Qt::OpenHandCursor));
}

void BoardItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!isEditable() || event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    if (hasSingleSelection()) {
        const Handle handle = handleAt(event->pos());
        if (handle != NoHandle) {
            beginResize(handle, event->scenePos());
            setCursor(cursorForHandle(handle));
            event->accept();
            return;
        }
    }

    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (modifiers & Qt::ControlModifier) {
        setSelected(!isSelected());
        if (!isSelected()) {
            event->accept();
            return;
        }
    } else if (!isSelected()) {
        if (!(modifiers & Qt::ShiftModifier))
            scene()->clearSelection();
        setSelected(true);
    }

    beginMove(event->scenePos());
    setCursor(QCursor(Qt::ClosedHandCursor));
    event->accept();
}

void BoardItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_drag == DragKind::None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    if (m_drag == DragKind::Move)
        applyMove(event->scenePos());
    else
        applyResize(event->scenePos(), event->modifiers());

    event->accept();
}

void BoardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_drag == DragKind::None) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    finishInteraction();
    updateHoverCursor(event->pos());
    event->accept();
}

QVariant BoardItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged)
        update();
    return QGraphicsObject::itemChange(change, value);
}

void BoardItem::beginMove(const QPointF& scenePos)
{
    m_drag = DragKind::Move;
    m_dragStartScene = scenePos;
    m_dragStartPos = pos();
    m_dragStartSize = m_size;
    m_before.clear();

    if (scene()) {
        const QList<QGraphicsItem*> selected = scene()->selectedItems();
        for (QGraphicsItem* item : selected) {
            auto* boardItem = qgraphicsitem_cast<BoardItem*>(item);
            if (boardItem)
                m_before.append({ boardItem, boardItem->pos(), boardItem->contentSize() });
        }
    }
    if (m_before.isEmpty())
        m_before.append({ this, pos(), m_size });
}

void BoardItem::applyMove(const QPointF& scenePos)
{
    const QPointF delta = scenePos - m_dragStartScene;
    for (const ItemGeometry& geometry : std::as_const(m_before)) {
        if (geometry.item)
            geometry.item->setPos(geometry.pos + delta);
    }
}

void BoardItem::beginResize(Handle handle, const QPointF& scenePos)
{
    m_drag = DragKind::Resize;
    m_resizeHandle = handle;
    m_dragStartScene = scenePos;
    m_dragStartPos = pos();
    m_dragStartSize = m_size;
    m_before = { { this, pos(), m_size } };
}

void BoardItem::applyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
{
    const QPointF delta = scenePos - m_dragStartScene;
    QRectF rect(m_dragStartPos, m_dragStartSize);

    if (isLeftHandle(m_resizeHandle))
        rect.setLeft(rect.left() + delta.x());
    if (isRightHandle(m_resizeHandle))
        rect.setRight(rect.right() + delta.x());
    if (isTopHandle(m_resizeHandle))
        rect.setTop(rect.top() + delta.y());
    if (isBottomHandle(m_resizeHandle))
        rect.setBottom(rect.bottom() + delta.y());

    const qreal minSide = kMinimumSide;
    if (rect.width() < minSide) {
        if (isLeftHandle(m_resizeHandle))
            rect.setLeft(rect.right() - minSide);
        else
            rect.setRight(rect.left() + minSide);
    }
    if (rect.height() < minSide) {
        if (isTopHandle(m_resizeHandle))
            rect.setTop(rect.bottom() - minSide);
        else
            rect.setBottom(rect.top() + minSide);
    }

    if ((modifiers & Qt::ShiftModifier) && isCornerHandle(m_resizeHandle)
        && m_dragStartSize.height() > 0.0) {
        const qreal aspect = m_dragStartSize.width() / m_dragStartSize.height();
        QSizeF size = rect.size();
        if (size.width() / size.height() > aspect)
            size.setWidth(size.height() * aspect);
        else
            size.setHeight(size.width() / aspect);
        size = size.expandedTo(QSizeF(minSide, minSide));

        if (isLeftHandle(m_resizeHandle))
            rect.setLeft(rect.right() - size.width());
        else
            rect.setRight(rect.left() + size.width());
        if (isTopHandle(m_resizeHandle))
            rect.setTop(rect.bottom() - size.height());
        else
            rect.setBottom(rect.top() + size.height());
    }

    setPos(rect.topLeft());
    setContentSize(rect.size());
}

void BoardItem::finishInteraction()
{
    if (m_drag == DragKind::None)
        return;

    const bool wasResize = m_drag == DragKind::Resize;
    m_drag = DragKind::None;
    m_resizeHandle = NoHandle;

    if (!m_before.isEmpty()) {
        ItemGeometryList after;
        after.reserve(m_before.size());
        for (const ItemGeometry& geometry : std::as_const(m_before)) {
            if (geometry.item)
                after.append({ geometry.item, geometry.item->pos(), geometry.item->contentSize() });
        }
        if (BoardScene* board = boardScene()) {
            board->pushGeometry(m_before, after,
                                wasResize ? QStringLiteral("缩放图元")
                                          : QStringLiteral("移动图元"));
        }
    }
    m_before.clear();
}

QJsonObject BoardItem::baseJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("type"), kind());
    json.insert(QStringLiteral("x"), pos().x());
    json.insert(QStringLiteral("y"), pos().y());
    json.insert(QStringLiteral("w"), m_size.width());
    json.insert(QStringLiteral("h"), m_size.height());
    json.insert(QStringLiteral("layer"), m_layerId);
    json.insert(QStringLiteral("height"), m_height);
    // Kept for readability and for older readers; the scene always derives
    // the actual paint order from layer + height.
    json.insert(QStringLiteral("z"), zValue());
    return json;
}

void BoardItem::applyBaseJson(const QJsonObject& json)
{
    setPos(json.value(QStringLiteral("x")).toDouble(),
           json.value(QStringLiteral("y")).toDouble());
    setContentSize(QSizeF(json.value(QStringLiteral("w")).toDouble(m_size.width()),
                          json.value(QStringLiteral("h")).toDouble(m_size.height())));
    m_layerId = qMax(0, json.value(QStringLiteral("layer")).toInt(0));
    m_height = json.contains(QStringLiteral("height"))
        ? qMax(0, json.value(QStringLiteral("height")).toInt(0))
        : -1;
}
