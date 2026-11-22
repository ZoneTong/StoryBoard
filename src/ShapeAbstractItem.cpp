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

bool ShapeAbstractItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    // 顶点 C 的内角手柄只属于三角形；自定义轮廓没有角度语义，不提供专用拖拽。
    if (handle != CanvasItem::CUSTOM_0 || shapeType() != ShapeType::TRIANGLE || IsCustomShape()) {
        return false;
    }
    m_apexDragBaseA = mapToScene(triangleVertex(TriangleVertex::A));
    m_apexDragBaseB = mapToScene(triangleVertex(TriangleVertex::B));
    m_apexDragHeight = qMax<qreal>(1.0, ContentSize().height());
    SetAngleReadoutVisible(true);
    return true;
}

bool ShapeAbstractItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 || shapeType() != ShapeType::TRIANGLE || IsCustomShape()) {
        return false;
    }
    // §3.1：拖顶点只改内角 A。AB 边上的垂直高度保持拖拽开始时的值，B 由底边长度
    // 与高度反算（cotB = base/h − cotA）；A 的钝角分支跟指针方向一致（D12），按 1° 取整。
    const QPointF base = m_apexDragBaseB - m_apexDragBaseA;
    const qreal baseLength = qMax<qreal>(1.0, std::hypot(base.x(), base.y()));
    const QPointF along = base / baseLength;
    const qreal fromA = QPointF::dotProduct(scenePos - m_apexDragBaseA, along);
    const qreal angleA = qRound(qRadiansToDegrees(qAtan2(m_apexDragHeight, fromA)));
    const qreal cotA = 1.0 / qTan(qDegreesToRadians(angleA));
    const qreal cotB = baseLength / m_apexDragHeight - cotA;
    const qreal angleB = qRadiansToDegrees(qAtan2(1.0, cotB));
    SetTriangleAnglesFixedHeight(angleA, angleB, m_apexDragHeight);
    SetAngleReadoutVisible(true);
    // 底边长度与高度都没变，把局部 A 点搬回拖拽开始时的场景位置即可，
    // B 会随同一条底边一起保持固定。
    const QPointF newA = mapToScene(triangleVertex(TriangleVertex::A));
    setPos(pos() + (m_apexDragBaseA - newA));
    return true;
}

void ShapeAbstractItem::EndHandleDrag()
{
    // 角度读数只在拖动期间显示。
    SetAngleReadoutVisible(false);
}

void ShapeAbstractItem::FrameResizeDragged()
{
    // §3.1：三角形四角 / 边自由缩放时，A、B 读数随拖动显示（B 在 ContentResized 里反算）。
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        SetAngleReadoutVisible(true);
    }
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
    update();
}

void ShapeAbstractItem::SetTriangleAnglesFixedHeight(qreal angle1, qreal angle2, qreal height)
{
    TriangleGeometry::ClampAngles(&angle1, &angle2);
    // §3.1 拖顶点：AB 边上的垂直高度是输入，底边与包围框宽度随角度派生。
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
    m_syncingTriangle = true;
    SetContentSize(TriangleGeometry::SizeForHeight(qMax<qreal>(1.0, height), angle1, angle2));
    m_syncingTriangle = false;
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

void ShapeAbstractItem::ContentResized()
{
    if (shapeType() != ShapeType::TRIANGLE || IsCustomShape()) {
        return;
    }
    if (m_syncingTriangle) {
        return;
    }
    // §3.1：四角自由缩放后 A 保持不变，B 按新框宽高反算（cotB = w/h − cotA）；
    // 反算分支随新框宽高自然取值，绘制按新角度贴合包围盒。
    const QSizeF size = ContentSize();
    const qreal cotA = 1.0 / qTan(qDegreesToRadians(m_bottomAngle1));
    const qreal cotB = size.width() / qMax<qreal>(1.0, size.height()) - cotA;
    const qreal angleB = qRadiansToDegrees(qAtan2(1.0, cotB));
    m_bottomAngle2 = qBound<qreal>(5.0, angleB, 175.0 - m_bottomAngle1);
    update();
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
        const QString textA = QStringLiteral("%1°").arg(qRound(BottomAngle1()));
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
    // 圆角 / 底角从属性扩展回放；没有这些键的图形保持当前值，不产生额外修改。
    if (const auto cornerIt = state.propertyExtras.constFind(ItemProperty::CORNER_RADIUS);
        cornerIt != state.propertyExtras.constEnd()) {
        m_cornerRadius = cornerIt->toDouble();
    }
    const qreal oldAngle1 = m_bottomAngle1;
    const qreal oldAngle2 = m_bottomAngle2;
    qreal angle1 = state.propertyExtras.value(ItemProperty::BOTTOM_ANGLE1, m_bottomAngle1).toDouble();
    qreal angle2 = state.propertyExtras.value(ItemProperty::BOTTOM_ANGLE2, m_bottomAngle2).toDouble();
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        TriangleGeometry::ClampAngles(&angle1, &angle2);
    }
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
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
