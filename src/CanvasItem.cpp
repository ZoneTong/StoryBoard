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
constexpr qreal MINIMUM_SIDE = 8.0;
} // namespace

QPointF CanvasItem::CustomHandleCenter(int handle, bool* ok) const
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
    setTransformOriginPoint(ContentRect().center());
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
            canvasScene->ForgetItem(this);
        }
    }
    delete m_handles;
    delete m_interaction;
}

QString CanvasItem::Name() const
{
    const QString trimmed = m_name.trimmed();
    return trimmed.isEmpty() ? TypeDisplayName() : trimmed;
}

void CanvasItem::SetName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (m_name == trimmed) {
        return;
    }
    m_name = trimmed;
}

QSizeF CanvasItem::MinimumSize()
{
    return QSizeF(MINIMUM_SIDE, MINIMUM_SIDE);
}

QRectF CanvasItem::ContentRect() const
{
    return CanvasItemGeometry::ContentRect(m_size);
}

QRectF CanvasItem::FrameRect() const
{
    return ContentRect();
}

void CanvasItem::SetContentSize(const QSizeF& size)
{
    const QSizeF requested = AdjustedContentSize(size);
    const QSizeF clamped(qMax(MINIMUM_SIDE, requested.width()), qMax(MINIMUM_SIDE, requested.height()));
    if (qFuzzyCompare(clamped.width(), m_size.width()) && qFuzzyCompare(clamped.height(), m_size.height())) {
        return;
    }
    prepareGeometryChange();
    m_size = clamped;
    // Rotation uses the content centre as its origin; keep it in sync after
    // interactive or property-dialog resizing.
    setTransformOriginPoint(ContentRect().center());
    update();
    ContentResized();
}

void CanvasItem::SetItemOpacity(qreal opacity)
{
    const qreal clamped = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_itemOpacity, clamped)) {
        return;
    }
    m_itemOpacity = clamped;
    update();
}

void CanvasItem::SetLayerId(int layerId)
{
    m_layerId = qMax(0, layerId);
}

void CanvasItem::SetZOrder(int height)
{
    m_zOrder = qMax(0, height);
}

void CanvasItem::SetRotationAngle(qreal angle)
{
    if (qFuzzyCompare(angle, m_rotation)) {
        return;
    }
    m_rotation = angle;
    setRotation(angle);
    update();
}

qreal CanvasItem::NormalizeRotation(qreal angle)
{
    while (angle >= 180.0) {
        angle -= 360.0;
    }
    while (angle < -180.0) {
        angle += 360.0;
    }
    return angle;
}

qreal CanvasItem::SnapRotation(qreal angle, qreal step)
{
    if (step <= 0.0) {
        return NormalizeRotation(angle);
    }
    return NormalizeRotation(qRound(angle / step) * step);
}

QRectF CanvasItem::SceneContentRect() const
{
    return mapRectToScene(ContentRect());
}

QRectF CanvasItem::boundingRect() const
{
    // 绘制区域仍是内容矩形；选中框可能更小（扇形 / 弧线），两者取并集。
    const QRectF area = ContentRect().united(FrameRect());
    return CanvasItemGeometry::BoundingRect(area, FramePadding(), isSelected(), ViewScale(), m_handles->Size(),
                                            m_handles->RotationOffset());
}

void CanvasItem::RefreshViewDependentGeometry()
{
    prepareGeometryChange();
    update();
}

QPainterPath CanvasItem::shape() const
{
    QPainterPath path;
    path.addRect(ContentRect());
    path.addPath(SelectionHandleShape());
    return path;
}

QPainterPath CanvasItem::OutlineShape() const
{
    QPainterPath path;
    path.addRect(ContentRect());
    return path;
}

QPainterPath CanvasItem::PaintedShape() const
{
    // 基类按内容矩形整体涂满处理：图片与文字等子类会覆写这里。
    return OutlineShape();
}

QPainterPath CanvasItem::ClippingShape() const
{
    // 半透明内容不能整块遮挡下层，否则 40% 的红矩形下面会露出画板底色而不是
    // 下层图形；这时让下层按原样参与合成，由透明度自然混色。
    if (m_itemOpacity < 1.0) {
        return QPainterPath();
    }
    return PaintedShape();
}

QPainterPath CanvasItem::SelectionHandleShape() const
{
    return m_handles->SelectionShape();
}

void CanvasItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 透明度只作用于内容，选中框与手柄在恢复后按不透明绘制。
    painter->setOpacity(painter->opacity() * m_itemOpacity);
    PaintContent(painter);
    painter->restore();

    if (isSelected() && !IsExporting()) {
        painter->save();
        DrawSelectionOverlay(painter);
        painter->restore();
    }
}

bool CanvasItem::IsEditable() const
{
    const CanvasScene* canvas = canvasScene();
    return canvas && canvas->IsItemEditable(this);
}

bool CanvasItem::IsExporting() const
{
    const CanvasScene* canvas = canvasScene();
    return canvas && canvas->IsExporting();
}

CanvasScene* CanvasItem::canvasScene() const
{
    return dynamic_cast<CanvasScene*>(scene());
}

qreal CanvasItem::ViewScale() const
{
    if (scene() && !scene()->views().isEmpty()) {
        const qreal scale = scene()->views().constFirst()->transform().m11();
        return qMax<qreal>(0.0001, scale);
    }
    return 1.0;
}

bool CanvasItem::HasSingleSelection() const
{
    return scene() && isSelected() && scene()->selectedItems().size() == 1;
}

qreal CanvasItem::HandleSize() const
{
    return m_handles->Size();
}

QPointF CanvasItem::HandleCenter(Handle handle) const
{
    return m_handles->Center(handle);
}

qreal CanvasItem::RotationHandleOffset() const
{
    return m_handles->RotationOffset();
}

QRectF CanvasItem::HandleRect(Handle handle) const
{
    return m_handles->Rect(handle);
}

CanvasItem::Handle CanvasItem::HandleAt(const QPointF& pos) const
{
    return static_cast<Handle>(m_handles->HitAt(pos));
}

QCursor CanvasItem::CursorForHandle(Handle handle) const
{
    return m_handles->Cursor(handle);
}

void CanvasItem::DrawSelectionOverlay(QPainter* painter)
{
    m_handles->DrawOverlay(painter);
}

void CanvasItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    m_interaction->OnHoverMove(event);
}

void CanvasItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_interaction->OnHoverLeave(event);
}

void CanvasItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    m_interaction->OnMousePress(event);
}

void CanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    m_interaction->OnMouseMove(event);
}

void CanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_interaction->OnMouseRelease(event);
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

ItemPropertySet CanvasItem::Properties() const
{
    // 所有图元共有的属性：名称 / 图层 / 层内叠放 / 位置 / 尺寸 / 旋转 / 不透明度。
    return ItemPropertySet{ItemProperty::NAME, ItemProperty::LAYER,    ItemProperty::HEIGHT, ItemProperty::POSITION,
                           ItemProperty::SIZE, ItemProperty::ROTATION, ItemProperty::OPACITY};
}

QVariant CanvasItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::NAME:
        return Name();
    case ItemProperty::LAYER:
        return LayerId();
    case ItemProperty::HEIGHT:
        return ZOrder();
    case ItemProperty::POSITION:
        return pos();
    case ItemProperty::SIZE:
        return ContentSize();
    case ItemProperty::ROTATION:
        return RotationAngle();
    case ItemProperty::OPACITY:
        return ItemOpacity();
    default:
        return QVariant();
    }
}

void CanvasItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::NAME:
        SetName(value.toString());
        break;
    case ItemProperty::LAYER:
        SetLayerId(value.toInt());
        break;
    case ItemProperty::HEIGHT:
        SetZOrder(value.toInt());
        break;
    case ItemProperty::POSITION:
        setPos(value.toPointF());
        break;
    case ItemProperty::SIZE:
        SetContentSize(value.toSizeF());
        break;
    case ItemProperty::ROTATION:
        SetRotationAngle(value.toDouble());
        break;
    case ItemProperty::OPACITY:
        SetItemOpacity(value.toDouble());
        break;
    default:
        break;
    }
}

QJsonObject CanvasItem::BaseJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("kind"), Kind());
    json.insert(QStringLiteral("x"), pos().x());
    json.insert(QStringLiteral("y"), pos().y());
    json.insert(QStringLiteral("w"), m_size.width());
    json.insert(QStringLiteral("h"), m_size.height());
    json.insert(QStringLiteral("layer"), m_layerId);
    json.insert(QStringLiteral("zOrder"), m_zOrder);
    if (m_groupId > 0) {
        json.insert(QStringLiteral("group"), m_groupId);
    }
    if (!qFuzzyIsNull(m_rotation)) {
        json.insert(QStringLiteral("rotation"), m_rotation);
    }
    // 透明度现在对所有图元生效，和几何一样写进 baseJson；1.0 不落键。
    if (!qFuzzyCompare(m_itemOpacity, 1.0)) {
        json.insert(QStringLiteral("opacity"), m_itemOpacity);
    }
    if (!m_name.isEmpty()) {
        json.insert(QStringLiteral("name"), m_name);
    }
    // 绘制顺序由 layer + height 决定，不再写冗余的 z 键。
    return json;
}

void CanvasItem::ApplyBaseJson(const QJsonObject& json)
{
    setPos(json.value(QStringLiteral("x")).toDouble(), json.value(QStringLiteral("y")).toDouble());
    SetContentSize(QSizeF(json.value(QStringLiteral("w")).toDouble(m_size.width()),
                          json.value(QStringLiteral("h")).toDouble(m_size.height())));
    m_layerId = qMax(0, json.value(QStringLiteral("layer")).toInt(0));
    m_groupId = qMax(0, json.value(QStringLiteral("group")).toInt(0));
    m_zOrder = json.contains(QStringLiteral("zOrder")) ? qMax(0, json.value(QStringLiteral("height")).toInt(0)) : -1;
    SetRotationAngle(json.value(QStringLiteral("rotation")).toDouble(0.0));
    SetItemOpacity(json.value(QStringLiteral("opacity")).toDouble(1.0));
    m_name = json.value(QStringLiteral("name")).toString().trimmed();
}
