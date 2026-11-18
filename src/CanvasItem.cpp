#include "CanvasItem.h"

#include "CanvasItemGeometry.h"
#include "CanvasItemHandles.h"
#include "CanvasItemInteraction.h"
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
} // namespace

QPointF CanvasItem::customHandleCenter(int handle, bool* ok) const
{
    Q_UNUSED(handle);
    if (ok) {
        *ok = false;
    }
    return QPointF();
}

CanvasItem::CanvasItem(QGraphicsItem* parent) : QGraphicsObject(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, false);
    setAcceptHoverEvents(false);
    setCursor(Qt::ArrowCursor);
    setTransformOriginPoint(contentRect().center());
    m_handles = new CanvasItemHandles(this);
    m_interaction = new CanvasItemInteraction(this);
}

CanvasItem::~CanvasItem()
{
    // 场景的选中排序缓存按 QPointer 惰性清理，依赖图元销毁后发出
    // selectionChanged 时再扫列表；多选中图元接连析构时，先析构者会触发
    // 扫描并释放观测数据，后析构者对同一批悬垂条目的读取会段错误。
    // 趁 QObject 部分仍完好时主动把自身清出缓存，杜绝这类悬垂读取。
    if (QGraphicsScene* owner = scene()) {
        if (auto* canvasScene = qobject_cast<CanvasScene*>(owner)) {
            canvasScene->forgetItem(this);
        }
    }
    delete m_handles;
    delete m_interaction;
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
    return CanvasItemGeometry::contentRect(m_size);
}

QRectF CanvasItem::frameRect() const
{
    return contentRect();
}

void CanvasItem::setContentSize(const QSizeF& size)
{
    const QSizeF requested = adjustedContentSize(size);
    const QSizeF clamped(qMax(kMinimumSide, requested.width()), qMax(kMinimumSide, requested.height()));
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

void CanvasItem::setItemOpacity(qreal opacity)
{
    QGraphicsItem::setOpacity(qBound<qreal>(0.0, opacity, 1.0));
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
    // 绘制区域仍是内容矩形；选中框可能更小（扇形 / 弧线），两者取并集。
    const QRectF area = contentRect().united(frameRect());
    return CanvasItemGeometry::boundingRect(area, framePadding(), isSelected(), viewScale(), m_handles->size(),
                                            m_handles->rotationOffset());
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

QPainterPath CanvasItem::clippingShape() const
{
    return paintedShape();
}

QPainterPath CanvasItem::selectionHandleShape() const
{
    return m_handles->selectionShape();
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
    return m_handles->size();
}

QPointF CanvasItem::handleCenter(Handle handle) const
{
    return m_handles->center(handle);
}

qreal CanvasItem::rotationHandleOffset() const
{
    return m_handles->rotationOffset();
}

QRectF CanvasItem::handleRect(Handle handle) const
{
    return m_handles->rect(handle);
}

CanvasItem::Handle CanvasItem::handleAt(const QPointF& pos) const
{
    return static_cast<Handle>(m_handles->hitAt(pos));
}

QCursor CanvasItem::cursorForHandle(Handle handle) const
{
    return m_handles->cursor(handle);
}

void CanvasItem::drawSelectionOverlay(QPainter* painter)
{
    m_handles->drawOverlay(painter);
}

void CanvasItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    m_interaction->onHoverMove(event);
}

void CanvasItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_interaction->onHoverLeave(event);
}

void CanvasItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_interaction->onMousePress(event);
}

void CanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    m_interaction->onMouseMove(event);
}

void CanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_interaction->onMouseRelease(event);
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
