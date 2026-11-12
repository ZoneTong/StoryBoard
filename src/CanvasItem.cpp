#include "CanvasItem.h"

#include "CanvasScene.h"
#include "I18n.h"

#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QJsonArray>
#include <QPainter>
#include <QSet>
#include <QStyleOptionGraphicsItem>
#include <QtMath>
#include <utility>

namespace {
constexpr qreal kMinimumSide = 8.0;
constexpr qreal kHandleHitPadding = 2.0;

bool isLeftHandle(CanvasItem::Handle handle)
{
    return handle == CanvasItem::TopLeft || handle == CanvasItem::Left || handle == CanvasItem::BottomLeft;
}

bool isRightHandle(CanvasItem::Handle handle)
{
    return handle == CanvasItem::TopRight || handle == CanvasItem::Right || handle == CanvasItem::BottomRight;
}

bool isTopHandle(CanvasItem::Handle handle)
{
    return handle == CanvasItem::TopLeft || handle == CanvasItem::Top || handle == CanvasItem::TopRight;
}

bool isBottomHandle(CanvasItem::Handle handle)
{
    return handle == CanvasItem::BottomLeft || handle == CanvasItem::Bottom || handle == CanvasItem::BottomRight;
}

bool isCornerHandle(CanvasItem::Handle handle)
{
    return handle == CanvasItem::TopLeft || handle == CanvasItem::TopRight || handle == CanvasItem::BottomLeft ||
           handle == CanvasItem::BottomRight;
}

} // namespace

CanvasItem::CanvasItem(QGraphicsItem* parent) : QGraphicsObject(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, false);
    setAcceptHoverEvents(false);
    setCursor(Qt::ArrowCursor);
    setTransformOriginPoint(contentRect().center());
}

QString CanvasItem::name() const
{
    const QString trimmed = m_name.trimmed();
    return trimmed.isEmpty() ? typeDisplayName() : trimmed;
}

void CanvasItem::setName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (m_name == trimmed) {
        return;
    }
    m_name = trimmed;
}

QSizeF CanvasItem::minimumSize()
{
    return QSizeF(kMinimumSide, kMinimumSide);
}

QRectF CanvasItem::contentRect() const
{
    return QRectF(0.0, 0.0, m_size.width(), m_size.height());
}

void CanvasItem::setContentSize(const QSizeF& size)
{
    const QSizeF clamped(qMax(kMinimumSide, size.width()), qMax(kMinimumSide, size.height()));
    if (qFuzzyCompare(clamped.width(), m_size.width()) && qFuzzyCompare(clamped.height(), m_size.height())) {
        return;
    }
    prepareGeometryChange();
    m_size = clamped;
    // Rotation uses the content centre as its origin; keep it in sync after
    // interactive or property-dialog resizing.
    setTransformOriginPoint(contentRect().center());
    update();
    contentResized();
}

void CanvasItem::setLayerId(int layerId)
{
    m_layerId = qMax(0, layerId);
}

void CanvasItem::setHeight(int height)
{
    m_height = qMax(0, height);
}

void CanvasItem::setRotationAngle(qreal angle)
{
    if (qFuzzyCompare(angle, m_rotation)) {
        return;
    }
    m_rotation = angle;
    setRotation(angle);
    update();
}

qreal CanvasItem::normalizeRotation(qreal angle)
{
    while (angle >= 180.0) {
        angle -= 360.0;
    }
    while (angle < -180.0) {
        angle += 360.0;
    }
    return angle;
}

qreal CanvasItem::snapRotation(qreal angle, qreal step)
{
    if (step <= 0.0) {
        return normalizeRotation(angle);
    }
    return normalizeRotation(qRound(angle / step) * step);
}

QRectF CanvasItem::sceneContentRect() const
{
    return mapRectToScene(contentRect());
}

QRectF CanvasItem::boundingRect() const
{
    const qreal padding = framePadding();
    qreal topPadding = padding;
    if (isSelected()) {
        // 旋转手柄画在内容区上方，其偏移随视图缩放变化。若 boundingRect
        // 不覆盖它，低缩放时 QGraphicsScene 的粗边界会先一步判定“未命中”，
        // 表现为旋转钮看得见却按不到、拖不动。
        const qreal knobRadius = handleSize() / 2.0 + 2.0 / qMax<qreal>(viewScale(), 0.001);
        topPadding = qMax(topPadding, rotationHandleOffset() + knobRadius);
    }
    return contentRect().adjusted(-padding, -topPadding, padding, padding);
}

void CanvasItem::refreshViewDependentGeometry()
{
    prepareGeometryChange();
    update();
}

QPainterPath CanvasItem::shape() const
{
    QPainterPath path;
    path.addRect(contentRect());
    path.addPath(selectionHandleShape());
    return path;
}

QPainterPath CanvasItem::outlineShape() const
{
    QPainterPath path;
    path.addRect(contentRect());
    return path;
}

QPainterPath CanvasItem::paintedShape() const
{
    // 基类按内容矩形整体涂满处理：图片与文字等子类会覆写这里。
    return outlineShape();
}

QPainterPath CanvasItem::selectionHandleShape() const
{
    QPainterPath path;
    if (!isSelected() || !isEditable() || !hasSingleSelection()) {
        return path;
    }

    // Selection handles are centred on the content corners, so half of every
    // handle lies outside the item geometry. Drawing them without adding them
    // to shape() leaves the visible handle unclickable; for outlines that never
    // reach the corner (ellipse, triangle, diamond, star) the handle could not
    // be grabbed at all.
    for (int handle = TopLeft; handle <= Rotation; ++handle) {
        path.addRect(handleRect(static_cast<Handle>(handle))
                         .adjusted(-kHandleHitPadding, -kHandleHitPadding, kHandleHitPadding, kHandleHitPadding));
    }
    return path;
}

void CanvasItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
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

bool CanvasItem::isEditable() const
{
    const CanvasScene* canvas = canvasScene();
    return canvas && canvas->isItemEditable(this);
}

bool CanvasItem::isExporting() const
{
    const CanvasScene* canvas = canvasScene();
    return canvas && canvas->isExporting();
}

CanvasScene* CanvasItem::canvasScene() const
{
    return dynamic_cast<CanvasScene*>(scene());
}

qreal CanvasItem::viewScale() const
{
    if (scene() && !scene()->views().isEmpty()) {
        const qreal scale = scene()->views().constFirst()->transform().m11();
        return qMax<qreal>(0.0001, scale);
    }
    return 1.0;
}

bool CanvasItem::hasSingleSelection() const
{
    return scene() && isSelected() && scene()->selectedItems().size() == 1;
}

qreal CanvasItem::handleSize() const
{
    return qBound<qreal>(6.0, 9.0 / viewScale(), 40.0);
}

QPointF CanvasItem::handleCenter(Handle handle) const
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
    case Rotation:
        return QPointF(rect.center().x(), rect.top() - rotationHandleOffset());
    case NoHandle:
        break;
    }
    return rect.center();
}

qreal CanvasItem::rotationHandleOffset() const
{
    // Keep the rotation handle one handle-size plus a small gap above the frame,
    // so it stays visually separate from the Top resize handle at any zoom.
    return handleSize() + 16.0 / viewScale();
}

QRectF CanvasItem::handleRect(Handle handle) const
{
    const qreal size = handleSize();
    const QPointF center = handleCenter(handle);
    return QRectF(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
}

CanvasItem::Handle CanvasItem::handleAt(const QPointF& pos) const
{
    const qreal hitPadding = qMin<qreal>(10.0 / viewScale(), 14.0);
    for (int handle = TopLeft; handle <= Rotation; ++handle) {
        const QRectF rect =
            handleRect(static_cast<Handle>(handle)).adjusted(-hitPadding, -hitPadding, hitPadding, hitPadding);
        if (rect.contains(pos)) {
            return static_cast<Handle>(handle);
        }
    }
    return NoHandle;
}

QCursor CanvasItem::cursorForHandle(Handle handle) const
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
    case Rotation:
        return QCursor(Qt::PointingHandCursor);
    case NoHandle:
        break;
    }
    return QCursor(Qt::ArrowCursor);
}

void CanvasItem::drawSelectionOverlay(QPainter* painter)
{
    QPen borderPen(QColor(79, 124, 255));
    borderPen.setWidthF(1.3);
    borderPen.setCosmetic(true);
    borderPen.setStyle(Qt::DashLine);
    borderPen.setDashPattern({5.0, 4.0});
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(contentRect());

    if (!isEditable() || !hasSingleSelection()) {
        return;
    }

    QPen handlePen(QColor(79, 124, 255));
    handlePen.setWidthF(1.4);
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int handle = TopLeft; handle <= Left; ++handle) {
        painter->drawRect(handleRect(static_cast<Handle>(handle)));
    }

    // Rotation handle: a circular knob above the frame, connected by a thin line.
    const QPointF topCenter(handleCenter(Top).x(), contentRect().top());
    const QPointF rotationCenter = handleCenter(Rotation);
    painter->drawLine(topCenter, rotationCenter);
    painter->setBrush(QColor(79, 124, 255));
    painter->drawEllipse(handleRect(Rotation).adjusted(1.0, 1.0, -1.0, -1.0));
}

void CanvasItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if (!isEditable()) {
        unsetCursor();
        QGraphicsObject::hoverMoveEvent(event);
        return;
    }
    updateHoverCursor(event->pos());
    QGraphicsObject::hoverMoveEvent(event);
}

void CanvasItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void CanvasItem::updateHoverCursor(const QPointF& pos)
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

void CanvasItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!isEditable() || event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    if (hasSingleSelection()) {
        const Handle handle = handleAt(event->pos());
        if (handle == Rotation) {
            beginRotate(event->scenePos());
            setCursor(QCursor(Qt::PointingHandCursor));
            event->accept();
            return;
        }
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
        if (!(modifiers & Qt::ShiftModifier)) {
            scene()->clearSelection();
        }
        setSelected(true);
    }

    beginMove(event->scenePos());
    setCursor(QCursor(Qt::ClosedHandCursor));
    event->accept();
}

void CanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_drag == DragKind::None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    if (m_drag == DragKind::Move) {
        applyMove(event->scenePos());
    } else if (m_drag == DragKind::Resize) {
        applyResize(event->scenePos(), event->modifiers());
    } else if (m_drag == DragKind::Rotate) {
        applyRotate(event->scenePos());
    }

    event->accept();
}

void CanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_drag == DragKind::None) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    finishInteraction();
    updateHoverCursor(event->pos());
    event->accept();
}

QVariant CanvasItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        // 选中后才绘制旋转手柄，boundingRect 的上边界随之变化。
        prepareGeometryChange();
        update();
    }
    return QGraphicsObject::itemChange(change, value);
}

void CanvasItem::beginMove(const QPointF& scenePos)
{
    m_drag = DragKind::Move;
    m_dragStartScene = scenePos;
    m_dragStartPos = pos();
    m_dragStartSize = m_size;
    m_before.clear();

    if (scene()) {
        QSet<CanvasItem*> seen;
        QSet<int> groupIds;
        auto addItem = [&seen](CanvasItem* item) {
            if (item && !seen.contains(item)) {
                seen.insert(item);
            }
        };
        const QList<QGraphicsItem*> selected = scene()->selectedItems();
        for (QGraphicsItem* item : selected) {
            auto* canvasItem = qgraphicsitem_cast<CanvasItem*>(item);
            if (!canvasItem) {
                continue;
            }
            if (!seen.contains(canvasItem)) {
                seen.insert(canvasItem);
            }
            if (canvasItem->groupId() > 0) {
                groupIds.insert(canvasItem->groupId());
            }
        }
        seen.insert(this);

        // 拖动任意一个组员时，把同组图元一并收进本次移动快照；组员不必处于
        // 选中状态，这样单个图元仍可打开属性窗并保持组内联动。
        if (CanvasScene* canvas = canvasScene()) {
            for (int groupId : std::as_const(groupIds)) {
                for (CanvasItem* member : canvas->itemsInGroup(groupId)) {
                    // 正常分组都同层；若图层后续被拆分或锁定，至少不要移动
                    // 当前不可编辑的组员，避免绕过图层锁定。
                    if (canvas->isItemEditable(member)) {
                        addItem(member);
                    }
                }
            }
        }

        for (CanvasItem* canvasItem : std::as_const(seen)) {
            m_before.append({canvasItem, canvasItem->pos(), canvasItem->contentSize()});
        }
    }
    if (m_before.isEmpty()) {
        m_before.append({this, pos(), m_size});
    }
}

void CanvasItem::applyMove(const QPointF& scenePos)
{
    const QPointF delta = scenePos - m_dragStartScene;
    for (const ItemGeometry& geometry : std::as_const(m_before)) {
        if (geometry.item) {
            geometry.item->setPos(geometry.pos + delta);
        }
    }
}

void CanvasItem::beginResize(Handle handle, const QPointF& scenePos)
{
    m_drag = DragKind::Resize;
    m_resizeHandle = handle;
    m_dragStartScene = scenePos;
    m_dragStartPos = pos();
    m_dragStartSize = m_size;
    m_before = {{this, pos(), m_size}};
}

void CanvasItem::applyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers)
{
    const QPointF delta = scenePos - m_dragStartScene;
    QRectF rect(m_dragStartPos, m_dragStartSize);

    if (isLeftHandle(m_resizeHandle)) {
        rect.setLeft(rect.left() + delta.x());
    }
    if (isRightHandle(m_resizeHandle)) {
        rect.setRight(rect.right() + delta.x());
    }
    if (isTopHandle(m_resizeHandle)) {
        rect.setTop(rect.top() + delta.y());
    }
    if (isBottomHandle(m_resizeHandle)) {
        rect.setBottom(rect.bottom() + delta.y());
    }

    const qreal minSide = kMinimumSide;
    if (rect.width() < minSide) {
        if (isLeftHandle(m_resizeHandle)) {
            rect.setLeft(rect.right() - minSide);
        } else {
            rect.setRight(rect.left() + minSide);
        }
    }
    if (rect.height() < minSide) {
        if (isTopHandle(m_resizeHandle)) {
            rect.setTop(rect.bottom() - minSide);
        } else {
            rect.setBottom(rect.top() + minSide);
        }
    }

    if ((modifiers & Qt::ShiftModifier) && isCornerHandle(m_resizeHandle) && m_dragStartSize.height() > 0.0) {
        const qreal aspect = m_dragStartSize.width() / m_dragStartSize.height();
        QSizeF size = rect.size();
        if (size.width() / size.height() > aspect) {
            size.setWidth(size.height() * aspect);
        } else {
            size.setHeight(size.width() / aspect);
        }
        size = size.expandedTo(QSizeF(minSide, minSide));

        if (isLeftHandle(m_resizeHandle)) {
            rect.setLeft(rect.right() - size.width());
        } else {
            rect.setRight(rect.left() + size.width());
        }
        if (isTopHandle(m_resizeHandle)) {
            rect.setTop(rect.bottom() - size.height());
        } else {
            rect.setBottom(rect.top() + size.height());
        }
    }

    setPos(rect.topLeft());
    setContentSize(rect.size());
}

void CanvasItem::beginRotate(const QPointF& scenePos)
{
    m_drag = DragKind::Rotate;
    m_dragStartScene = scenePos;
    m_dragStartRotation = m_rotation;
    const QPointF center = mapToScene(contentRect().center());
    m_rotateStartAngle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    m_before.clear();
}

void CanvasItem::applyRotate(const QPointF& scenePos)
{
    const QPointF center = mapToScene(contentRect().center());
    const qreal angle = qRadiansToDegrees(qAtan2(scenePos.y() - center.y(), scenePos.x() - center.x()));
    const qreal target = m_dragStartRotation + (angle - m_rotateStartAngle);
    // 需求 35：旋转手柄按 5° 步长吸附。
    setRotationAngle(snapRotation(target, 5.0));
}

void CanvasItem::finishInteraction()
{
    if (m_drag == DragKind::None) {
        return;
    }

    const bool wasResize = m_drag == DragKind::Resize;
    const bool wasRotate = m_drag == DragKind::Rotate;
    m_drag = DragKind::None;
    m_resizeHandle = NoHandle;

    if (wasRotate) {
        if (CanvasScene* canvas = canvasScene()) {
            canvas->pushRotation(this, m_dragStartRotation, m_rotation, tr("Rotate item"));
        }
    } else if (!m_before.isEmpty()) {
        ItemGeometryList after;
        after.reserve(m_before.size());
        for (const ItemGeometry& geometry : std::as_const(m_before)) {
            if (geometry.item) {
                after.append({geometry.item, geometry.item->pos(), geometry.item->contentSize()});
            }
        }
        if (CanvasScene* canvas = canvasScene()) {
            canvas->pushGeometry(m_before, after, wasResize ? tr("Resize item") : tr("Move item"));
        }
    }
    m_before.clear();
}

QJsonObject CanvasItem::baseJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("type"), kind());
    json.insert(QStringLiteral("x"), pos().x());
    json.insert(QStringLiteral("y"), pos().y());
    json.insert(QStringLiteral("w"), m_size.width());
    json.insert(QStringLiteral("h"), m_size.height());
    json.insert(QStringLiteral("layer"), m_layerId);
    json.insert(QStringLiteral("height"), m_height);
    if (m_groupId > 0) {
        json.insert(QStringLiteral("group"), m_groupId);
    }
    if (!qFuzzyIsNull(m_rotation)) {
        json.insert(QStringLiteral("rotation"), m_rotation);
    }
    if (!m_name.isEmpty()) {
        json.insert(QStringLiteral("itemName"), m_name);
    }
    // Kept for readability and for older readers; the scene always derives
    // the actual paint order from layer + height.
    json.insert(QStringLiteral("z"), zValue());
    return json;
}

void CanvasItem::applyBaseJson(const QJsonObject& json)
{
    setPos(json.value(QStringLiteral("x")).toDouble(), json.value(QStringLiteral("y")).toDouble());
    setContentSize(QSizeF(json.value(QStringLiteral("w")).toDouble(m_size.width()),
                          json.value(QStringLiteral("h")).toDouble(m_size.height())));
    m_layerId = qMax(0, json.value(QStringLiteral("layer")).toInt(0));
    m_groupId = qMax(0, json.value(QStringLiteral("group")).toInt(0));
    m_height = json.contains(QStringLiteral("height")) ? qMax(0, json.value(QStringLiteral("height")).toInt(0)) : -1;
    setRotationAngle(json.value(QStringLiteral("rotation")).toDouble(0.0));
    m_name = json.value(QStringLiteral("itemName")).toString().trimmed();
}
