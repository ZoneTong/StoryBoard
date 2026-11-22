#include "ShapeAbstractItem.h"

#include "ArrowStyle.h"
#include "CanvasItemVisitor.h"
#include "I18n.h"
#include "ShapeHitGeometry.h"
#include "ShapeMeta.h"
#include "ShapePainter.h"
#include "ShapePathBuilder.h"
#include "TriangleGeometry.h"
#include "shapes/ShapeFactory.h"

#include <QByteArray>
#include <QDataStream>
#include <QFontMetricsF>
#include <QIODevice>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QtMath>


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
    // 椭圆 / 扇形的内容框即椭圆外接框，长短半轴可由尺寸派生、写回时反向换算。
    if (!IsCustomShape() && (shapeType() == ShapeType::ELLIPSE || shapeType() == ShapeType::SECTOR)) {
        properties = properties.United(ItemPropertySet{ItemProperty::MAJOR_AXIS, ItemProperty::MINOR_AXIS});
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
    case ItemProperty::MAJOR_AXIS:
        return qMax(ContentSize().width(), ContentSize().height()) / 2.0;
    case ItemProperty::MINOR_AXIS:
        return qMin(ContentSize().width(), ContentSize().height()) / 2.0;
    case ItemProperty::BOTTOM_ANGLE1:
        return BottomAngle1();
    case ItemProperty::BOTTOM_ANGLE2:
        return BottomAngle2();
    case ItemProperty::PARALLELOGRAM_ANGLE:
        return ParallelogramAngle();
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
    case ItemProperty::MAJOR_AXIS:
    case ItemProperty::MINOR_AXIS: {
        // 半轴换算回内容宽高：长轴沿当前较长的边，另一根半轴保持不变，中心不动。
        const QSizeF size = ContentSize();
        const bool widthIsMajor = size.width() >= size.height();
        qreal major = widthIsMajor ? size.width() / 2.0 : size.height() / 2.0;
        qreal minor = widthIsMajor ? size.height() / 2.0 : size.width() / 2.0;
        if (property == ItemProperty::MAJOR_AXIS) {
            major = value.toDouble();
            if (major < minor) {
                minor = major;
            }
        } else {
            minor = value.toDouble();
            if (minor > major) {
                major = minor;
            }
        }
        const qreal newWidth = widthIsMajor ? major * 2.0 : minor * 2.0;
        const qreal newHeight = widthIsMajor ? minor * 2.0 : major * 2.0;
        const QPointF center = pos() + QPointF(size.width() / 2.0, size.height() / 2.0);
        setPos(center.x() - newWidth / 2.0, center.y() - newHeight / 2.0);
        SetContentSize(QSizeF(newWidth, newHeight));
        break;
    }
    case ItemProperty::BOTTOM_ANGLE1:
        SetTriangleAngles(value.toDouble(), BottomAngle2());
        break;
    case ItemProperty::BOTTOM_ANGLE2:
        SetTriangleAngles(BottomAngle1(), value.toDouble());
        break;
    case ItemProperty::PARALLELOGRAM_ANGLE:
        SetParallelogramAngle(value.toDouble());
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
    return ShapeTypeName(shapeType());
}

QPointF ShapeAbstractItem::triangleVertex(TriangleVertex vertex) const
{
    const TriangleGeometry::Vertices vertices = TriangleGeometry::Make(ContentRect(), m_bottomAngle1, m_bottomAngle2);
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

QPointF ShapeAbstractItem::CustomHandleCenter(int handle, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (handle == CanvasItem::APEX && shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        // 顶点 C 的手柄向下偏移，让开底角 90° 时与角顶点重合的角落手柄；
        // 命中与拖拽都仍按真实顶点所在的包围框计算，形很矮时收在内框里。
        const qreal scale = qMax<qreal>(ViewScale(), 0.001);
        const qreal inset = qMin<qreal>(18.0 / scale, qMax<qreal>(0.0, ContentRect().height() - 7.0 / scale));
        if (ok) {
            *ok = true;
        }
        return triangleVertex(TriangleVertex::C) + QPointF(0.0, inset);
    }
    return QPointF();
}

void ShapeAbstractItem::SetTriangleAngles(qreal angle1, qreal angle2)
{
    TriangleGeometry::ClampAngles(&angle1, &angle2);
    if (qFuzzyCompare(m_bottomAngle1 + 2.0, angle1 + 2.0) && qFuzzyCompare(m_bottomAngle2 + 2.0, angle2 + 2.0)) {
        return;
    }
    // 角度优先：底边长度保持不变，包围盒宽高按新角度重新派生。
    const QSizeF newSize =
        TriangleGeometry::SizeForAngles(ContentSize(), m_bottomAngle1, m_bottomAngle2, angle1, angle2);
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
    m_syncingTriangle = true;
    SetContentSize(newSize);
    m_syncingTriangle = false;
    m_triangleSyncSize = ContentSize();
    update();
}

void ShapeAbstractItem::SetAngleReadoutVisible(bool visible)
{
    if (m_angleReadoutVisible == visible) {
        return;
    }
    m_angleReadoutVisible = visible;
    update();
}

void ShapeAbstractItem::SetParallelogramAngle(qreal angle)
{
    const qreal clamped = qBound<qreal>(5.0, angle, 175.0);
    if (qFuzzyCompare(m_parallelogramAngle + 2.0, clamped + 2.0)) {
        return;
    }
    m_parallelogramAngle = clamped;
    // 角度只改上边相对底边的水平偏移，内容矩形宽高不变。
    prepareGeometryChange();
    update();
}

void ShapeAbstractItem::ContentResized()
{
    if (shapeType() != ShapeType::TRIANGLE || IsCustomShape()) {
        return;
    }
    // 尺寸折算已经在 AdjustedContentSize() 里做完，这里只记一次同步基准，
    // 不再在 resize 过程中二次改尺寸，避免拖动底边时反复重排导致闪烁。
    if (!m_syncingTriangle) {
        m_triangleSyncSize = ContentSize();
    }
}

QSizeF ShapeAbstractItem::AdjustedContentSize(const QSizeF& size) const
{
    if (shapeType() != ShapeType::TRIANGLE || IsCustomShape()) {
        return size;
    }
    const QSizeF current = ContentSize();
    const qreal ratio = TriangleGeometry::AspectRatio(m_bottomAngle1, m_bottomAngle2);
    const bool widthChanged = !qFuzzyCompare(size.width() + 0.01, current.width() + 0.01);
    const bool heightChanged = !qFuzzyCompare(size.height() + 0.01, current.height() + 0.01);
    QSizeF target = size;
    if (heightChanged && !widthChanged) {
        // 拖高等价于等比改底边：高度是本次输入，包围盒宽度按角度比例放大 / 缩小。
        target = TriangleGeometry::SizeForHeight(size.height(), m_bottomAngle1, m_bottomAngle2);
    } else if (widthChanged) {
        // 拖宽度：高度由包围盒宽度与角度派生。
        target.setHeight(size.width() / ratio);
    }
    return target;
}

QRectF ShapeAbstractItem::FrameRect() const
{
    // 扇形只画了内容矩形里的一小块，选中框与手柄改贴在轮廓的最小覆盖矩形上。
    if (m_customPath.isEmpty() && shapeType() == ShapeType::SECTOR) {
        const QPainterPath path = CurrentPath(ContentRect());
        const QRectF bounds = path.boundingRect();
        if (!bounds.isEmpty()) {
            return bounds;
        }
    }
    return CanvasItem::FrameRect();
}

void ShapeAbstractItem::DrawSelectionOverlay(QPainter* painter)
{
    CanvasItem::DrawSelectionOverlay(painter);
    if (shapeType() != ShapeType::TRIANGLE || IsCustomShape() || !isSelected() || !IsEditable() ||
        !HasSingleSelection()) {
        return;
    }
    // 底边中点的底边长度手柄两侧各补一根短刻度线，提示这里拖的是底边。
    const QRectF frame = FrameRect();
    const qreal gap = qMax<qreal>(4.0, 6.0 / qMax<qreal>(ViewScale(), 0.001));
    const qreal tick = qMax<qreal>(4.0, 7.0 / qMax<qreal>(ViewScale(), 0.001));
    QPen tickPen(QColor(79, 124, 255));
    tickPen.setWidthF(1.2);
    tickPen.setCosmetic(true);
    painter->save();
    painter->setPen(tickPen);
    painter->setBrush(Qt::NoBrush);
    const QPointF center(frame.center().x(), frame.bottom());
    painter->drawLine(QPointF(center.x() - gap, center.y()), QPointF(center.x() - gap, center.y() - tick));
    painter->drawLine(QPointF(center.x() + gap, center.y()), QPointF(center.x() + gap, center.y() - tick));
    if (AngleReadoutVisible()) {
        const qreal scale = qMax<qreal>(ViewScale(), 0.001);
        QFont font = painter->font();
        font.setPointSizeF(qMax<qreal>(8.0, 11.0 / scale));
        painter->setFont(font);
        painter->setPen(QColor(37, 99, 235));
        const QFontMetricsF metrics(font);
        const QPointF a = triangleVertex(TriangleVertex::A);
        const QPointF b = triangleVertex(TriangleVertex::B);
        const QString textA = QStringLiteral("%1°").arg(BottomAngle1(), 0, 'f', 1);
        const QString textB = QStringLiteral("%1°").arg(BottomAngle2(), 0, 'f', 1);
        painter->drawText(a + QPointF(6.0 / scale, -6.0 / scale), textA);
        painter->drawText(b + QPointF(-metrics.horizontalAdvance(textB) - 6.0 / scale, -6.0 / scale), textB);
    }
    painter->restore();
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
    result.cornerRadius = m_cornerRadius;
    result.bottomAngle1 = m_bottomAngle1;
    result.bottomAngle2 = m_bottomAngle2;
    result.parallelogramAngle = m_parallelogramAngle;
    result.pos = pos();
    result.size = ContentSize();
    WriteState(result);
    return result;
}

void ShapeAbstractItem::ApplyState(const ShapeItemState& state)
{
    ApplyFillState(state.fill);
    ApplyStrokeState(state.stroke);
    SetItemOpacity(state.opacity);
    m_cornerRadius = state.cornerRadius;
    const qreal oldAngle1 = m_bottomAngle1;
    const qreal oldAngle2 = m_bottomAngle2;
    qreal angle1 = state.bottomAngle1;
    qreal angle2 = state.bottomAngle2;
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        TriangleGeometry::ClampAngles(&angle1, &angle2);
    }
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
    m_parallelogramAngle = qBound<qreal>(5.0, state.parallelogramAngle, 175.0);
    setPos(state.pos);
    const QSizeF requested = state.size.isEmpty() ? DefaultContentSize() : state.size;
    const bool sameSize = qFuzzyCompare(requested.width() + 0.01, ContentSize().width() + 0.01) &&
                          qFuzzyCompare(requested.height() + 0.01, ContentSize().height() + 0.01);
    bool preserveTriangleBase = false;
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        const bool angleChanged =
            !qFuzzyCompare(oldAngle1 + 2.0, angle1 + 2.0) || !qFuzzyCompare(oldAngle2 + 2.0, angle2 + 2.0);
        preserveTriangleBase = sameSize && angleChanged;
    }
    m_syncingTriangle = true;
    if (preserveTriangleBase) {
        SetContentSize(TriangleGeometry::SizeForAngles(ContentSize(), oldAngle1, oldAngle2, angle1, angle2));
    } else {
        QSizeF target = requested;
        if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
            target.setHeight(target.width() / TriangleGeometry::AspectRatio(angle1, angle2));
        }
        SetContentSize(target);
    }
    m_syncingTriangle = false;
    ReadState(state);
    m_triangleSyncSize = ContentSize();
    update();
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

ShapeLayer ShapeAbstractItem::shapeLayer(int index) const
{
    Q_UNUSED(index);
    return ShapeLayer();
}

QPainterPath ShapeAbstractItem::MapShapeLayerPath(const ShapeLayer& layer) const
{
    return layer.path;
}

bool ShapeAbstractItem::HasShapeFillArea() const
{
    if (ShapeLayerCount() == 0) {
        return true;
    }
    for (int index = 0; index < ShapeLayerCount(); ++index) {
        const ShapeLayer layer = shapeLayer(index);
        if (layer.role == ShapeLayerRole::FILL_AREA && !layer.path.isEmpty()) {
            return true;
        }
    }
    return false;
}

void ShapeAbstractItem::WriteState(ShapeItemState& state) const
{
    Q_UNUSED(state);
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
