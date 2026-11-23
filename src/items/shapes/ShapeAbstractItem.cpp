#include "items/shapes/ShapeAbstractItem.h"

#include "app/I18n.h"
#include "items/CanvasItemVisitor.h"
#include "items/geometry/ShapeHitGeometry.h"
#include "items/geometry/ShapeMeta.h"
#include "items/geometry/ShapePainter.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/geometry/TriangleGeometry.h"
#include "items/properties/ArrowStyle.h"

#include <QByteArray>
#include <QDataStream>
#include <QFontMetricsF>
#include <QIODevice>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QtMath>
#include <cmath>


ShapeAbstractItem::ShapeAbstractItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
}

ItemPropertySet ShapeAbstractItem::Properties() const
{
    // 公共样式六件套；圆角 / 角度 / 箭头等按图形由子类自行追加。
    // 背景图片的摆放方式与图片本身同属填充属性，一并纳入属性全集，
    // 属性面板与属性窗都按这一份快照写回。
    ItemPropertySet properties = CanvasItem::Properties().United(ItemPropertySet{
        ItemProperty::FILL_STYLE, ItemProperty::FILL_COLOR, ItemProperty::FILL_IMAGE, ItemProperty::FILL_IMAGE_MODE,
        ItemProperty::STROKE_STYLE, ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH});
    // §3.4：中心点只上报给对称图形，属性窗与编辑面板据此决定这一行开不开。
    if (ShapeMeta::SupportsCenter(shapeType())) {
        properties.Insert(ItemProperty::CENTER);
    }
    return properties;
}

QSizeF ShapeAbstractItem::DefaultContentSize() const
{
    // 统一基准（需求 16）：棋盘格 1 格 = 64px，基准边长 = 2 格 = 128px。
    // 正方形 / 圆等直接用 128×128，异形比例由具体子类覆写。
    return QSizeF(128.0, 128.0);
}

QVariant ShapeAbstractItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::FILL_STYLE:
        return static_cast<int>(fillStyle());
    case ItemProperty::FILL_COLOR:
        return FillColor();
    case ItemProperty::FILL_IMAGE:
        return QVariant::fromValue(FillImage());
    case ItemProperty::FILL_IMAGE_MODE:
        return static_cast<int>(FillImageMode());
    case ItemProperty::STROKE_STYLE:
        return static_cast<int>(StrokeStyle());
    case ItemProperty::STROKE_COLOR:
        return StrokeColor();
    case ItemProperty::STROKE_WIDTH:
        return StrokeWidth();
    case ItemProperty::CORNER_RADIUS:
        return CornerRadius();
    case ItemProperty::BOTTOM_ANGLE1:
        return BottomAngle1();
    case ItemProperty::BOTTOM_ANGLE2:
        return BottomAngle2();
    default:
        return CanvasItem::PropertyValue(property);
    }
}

void ShapeAbstractItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::FILL_STYLE:
        SetFillStyle(static_cast<FillStyle>(value.toInt()));
        break;
    case ItemProperty::FILL_COLOR:
        SetFill(value.value<QColor>(), fillStyle());
        break;
    case ItemProperty::FILL_IMAGE: {
        const QPixmap image = value.value<QPixmap>();
        if (image.isNull()) {
            ClearFillImage();
        } else {
            SetFillImage(image, FillImageMode());
        }
        break;
    }
    case ItemProperty::FILL_IMAGE_MODE:
        SetFillImage(FillImage(), static_cast<ImageFill::Mode>(value.toInt()));
        break;
    case ItemProperty::STROKE_STYLE:
        SetStrokeStyle(static_cast<Qt::PenStyle>(value.toInt()));
        break;
    case ItemProperty::STROKE_COLOR:
        SetStyle(value.value<QColor>(), FillColor(), StrokeWidth());
        break;
    case ItemProperty::STROKE_WIDTH:
        SetStyle(StrokeColor(), FillColor(), value.toDouble());
        break;
    case ItemProperty::CORNER_RADIUS:
        SetCornerRadius(value.toDouble());
        break;
    case ItemProperty::BOTTOM_ANGLE1:
        SetTriangleAngles(value.toDouble(), BottomAngle2());
        break;
    case ItemProperty::BOTTOM_ANGLE2:
        SetTriangleAngles(BottomAngle1(), value.toDouble());
        break;
    default:
        CanvasItem::SetPropertyValue(property, value);
        break;
    }
}

void ShapeAbstractItem::Accept(CanvasItemVisitor& visitor)
{
    visitor.VisitShape(*this);
}

QString ShapeAbstractItem::DisplayName() const
{
    if (!m_customName.isEmpty()) {
        return m_customName;
    }
    return ShapeTypes::Name(shapeType());
}

QPointF ShapeAbstractItem::triangleVertex(TriangleVertex vertex) const
{
    const TriangleGeometry::Vertices vertices = TriangleGeometry::Make(ContentRect(), BottomAngle1(), BottomAngle2());
    switch (vertex) {
    case TriangleVertex::A:
        return vertices.a;
    case TriangleVertex::B:
        return vertices.b;
    case TriangleVertex::C:
    default:
        return vertices.c;
    }
}

void ShapeAbstractItem::SetTriangleAngles(qreal angle1, qreal angle2)
{
    // 非三角形图形没有底角属性，属性表中的这两项也不会出现在这里。
    Q_UNUSED(angle1);
    Q_UNUSED(angle2);
}

void ShapeAbstractItem::SetTriangleAnglesFixedHeight(qreal angle1, qreal angle2, qreal height)
{
    Q_UNUSED(angle1);
    Q_UNUSED(angle2);
    Q_UNUSED(height);
}

void ShapeAbstractItem::EndHandleDrag()
{
    // 角度读数只在拖动期间显示（三角形与平行四边形共用这一开关）。
    SetAngleReadoutVisible(false);
}

QRectF ShapeAbstractItem::BottomReadoutRect(const QString& text, Qt::Alignment alignment) const
{
    const QFontMetricsF metrics(ReadoutFont(QFont()));
    const QRectF content = ContentRect();
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    const qreal gap = HandleSize() / 2.0 + 4.0 / scale;
    const qreal width = metrics.horizontalAdvance(text);
    const qreal height = metrics.height();
    qreal left = content.center().x() - width / 2.0;
    if ((alignment & Qt::AlignLeft) != 0) {
        left = content.left();
    } else if ((alignment & Qt::AlignRight) != 0) {
        left = content.right() - width;
    }
    return QRectF(left, content.bottom() + gap, width, height);
}

qreal ShapeAbstractItem::FramePadding() const
{
    return qMax<qreal>(qMax<qreal>(22.0, StrokeWidth() + 6.0), ReadoutPadding());
}

QRectF ShapeAbstractItem::FrameRect() const
{
    // 扇形是“椭圆上截出的一块平面”，其内容框本来就是生成它的椭圆外接框，
    // 因此不再按可见轮廓的最小覆盖矩形收缩（§D12）；起止角改变不影响选框。
    return CanvasItem::FrameRect();
}

void ShapeAbstractItem::SetCustomShape(const QString& name, const QPainterPath& unitPath)
{
    m_customName = name;
    m_customPath = unitPath;
    if (!m_customPath.isEmpty()) {
        m_cornerRadius = 0.0;
    }
    update();
}

void ShapeAbstractItem::SetStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth)
{
    SetStroke(strokeColor, strokeWidth);
    SetFill(fillColor, fillStyle());
}

void ShapeAbstractItem::FillChanged()
{
    update();
}

void ShapeAbstractItem::StrokeChanged()
{
    // 描线宽度参与取景余量，句柄区域跟着变化，需要重算 boundingRect。
    prepareGeometryChange();
    update();
}

void ShapeAbstractItem::SetCornerRadius(qreal radius)
{
    // 圆角半径按画布像素处理，不超过短边的一半；0 表示直角。
    const qreal clamped = qBound<qreal>(0.0, radius, qMin(ContentRect().width(), ContentRect().height()) / 2.0);
    if (qFuzzyCompare(m_cornerRadius + 1.0, clamped + 1.0)) {
        return;
    }
    m_cornerRadius = clamped;
    update();
}

ShapeItemState ShapeAbstractItem::State() const
{
    ShapeItemState result;
    result.fill = FillState();
    result.stroke = StrokeState();
    result.opacity = ItemOpacity();
    result.pos = pos();
    result.size = ContentSize();
    // 公共字段写完后交给叶子：类型专属标量按 ItemProperty 进 propertyExtras。
    WriteState(result);
    return result;
}

void ShapeAbstractItem::ApplyState(const ShapeItemState& state)
{
    ApplyFillState(state.fill);
    ApplyStrokeState(state.stroke);
    SetItemOpacity(state.opacity);
    // 圆角从属性扩展回放；没有这个键的图形保持当前值，不产生额外修改。
    if (const auto cornerIt = state.propertyExtras.constFind(ItemProperty::CORNER_RADIUS);
        cornerIt != state.propertyExtras.constEnd()) {
        m_cornerRadius = cornerIt->toDouble();
    }
    setPos(state.pos);
    ApplyStateContentSize(state.size.isEmpty() ? DefaultContentSize() : state.size);
    ReadState(state);
    update();
}

void ShapeAbstractItem::ApplyStateContentSize(const QSizeF& requested)
{
    SetContentSize(requested);
}

QPainterPath ShapeAbstractItem::shape() const
{
    return ShapeHitGeometry::HitShape(CurrentPath(ContentRect()), shapeType(), m_strokeWidth, SelectionHandleShape());
}

QPainterPath ShapeAbstractItem::OutlineShape() const
{
    const QPainterPath path = CurrentPath(ContentRect());
    return path.isEmpty() ? CanvasItem::OutlineShape() : path;
}

QPainterPath ShapeAbstractItem::PaintedShape() const
{
    return ShapeHitGeometry::PaintedShape(CurrentPath(ContentRect()), shapeType(), m_fillStyle, m_fillColor,
                                          m_strokeWidth, m_strokeStyle);
}

QPainterPath ShapeAbstractItem::CurrentPath(const QRectF& rect) const
{
    if (!m_customPath.isEmpty()) {
        return ShapePathBuilder::MapUnitPath(m_customPath, rect);
    }
    return ShapePath(rect);
}

QPainterPath ShapeAbstractItem::PathFor(ShapeType type, const QRectF& rect)
{
    return PathFor(type, rect, 0.0, 30.0, 120.0);
}

QPainterPath ShapeAbstractItem::PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return PathFor(type, rect, cornerRadius, 30.0, 120.0);
}

QPainterPath ShapeAbstractItem::PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                        qreal sweepAngle)
{
    return ShapePathBuilder::PathFor(type, rect, cornerRadius, startAngle, sweepAngle);
}

void ShapeAbstractItem::PaintContent(QPainter* painter)
{
    const QRectF rect = ContentRect();
    if (rect.isEmpty()) {
        return;
    }

    const QPainterPath path = CurrentPath(rect);
    if (path.isEmpty()) {
        return;
    }

    // 需求 10：背景图片在填充样式之下：先铺图片，再由填充色 / 图案遮挡它。
    // 图片仍然裁剪到填充区并避开描边带，描线不会被打断。
    if (!m_fillImage.isNull()) {
        painter->save();
        QPainterPath clip = path;
        if (m_strokeWidth > 0.0 && m_strokeColor.alpha() > 0) {
            QPainterPathStroker stroker;
            stroker.setWidth(m_strokeWidth);
            stroker.setJoinStyle(Qt::MiterJoin);
            clip = path.subtracted(stroker.createStroke(path));
        }
        ImageFill::Paint(painter, m_fillImage, m_fillImageMode, rect, clip);
        painter->restore();
    }

    ShapePainter::PaintShape(painter, path, rect, shapeType(), m_strokeColor, m_fillColor, m_strokeWidth, m_strokeStyle,
                             m_fillStyle);
}

void ShapeAbstractItem::WriteState(ShapeItemState& state) const
{
    // 公共可编辑标量按属性表进入快照：没有该属性的图形不写键，避免“打开即修改”。
    const ItemPropertySet properties = Properties();
    if (properties.Contains(ItemProperty::CORNER_RADIUS)) {
        state.propertyExtras.insert(ItemProperty::CORNER_RADIUS, CornerRadius());
    }
    if (properties.Contains(ItemProperty::BOTTOM_ANGLE1)) {
        state.propertyExtras.insert(ItemProperty::BOTTOM_ANGLE1, BottomAngle1());
    }
    if (properties.Contains(ItemProperty::BOTTOM_ANGLE2)) {
        state.propertyExtras.insert(ItemProperty::BOTTOM_ANGLE2, BottomAngle2());
    }
}

void ShapeAbstractItem::ReadState(const ShapeItemState& state)
{
    Q_UNUSED(state);
}

void ShapeAbstractItem::WriteJsonExtra(QJsonObject& json) const
{
    Q_UNUSED(json);
}

void ShapeAbstractItem::ReadJsonExtra(const QJsonObject& json)
{
    Q_UNUSED(json);
}
