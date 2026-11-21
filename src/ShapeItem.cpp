#include "ShapeItem.h"

#include "ArrowStyle.h"
#include "I18n.h"
#include "ShapeHitGeometry.h"
#include "ShapeMeta.h"
#include "ShapePainter.h"
#include "ShapePathBuilder.h"
#include "shapes/ShapeFactory.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QtMath>


ShapeItem::ShapeItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
}

ItemPropertySet ShapeItem::Properties() const
{
    // 公共样式六件套；圆角 / 角度 / 箭头等按图形由子类自行追加。
    // 背景图片的摆放方式与图片本身同属填充属性，一并纳入属性全集，
    // 属性面板与属性窗都按这一份快照写回。
    return CanvasItem::Properties().United(ItemPropertySet{
        ItemProperty::FILL_STYLE, ItemProperty::FILL_COLOR, ItemProperty::FILL_IMAGE, ItemProperty::FILL_IMAGE_MODE,
        ItemProperty::STROKE_STYLE, ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH});
}

QSizeF ShapeItem::DefaultContentSize() const
{
    // 统一基准（需求 16）：棋盘格 1 格 = 64px，基准边长 = 2 格 = 128px。
    // 正方形 / 圆等直接用 128×128，异形比例由具体子类覆写。
    return QSizeF(128.0, 128.0);
}

QVariant ShapeItem::PropertyValue(ItemProperty property) const
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
    case ItemProperty::START_ANGLE:
        return StartAngle();
    case ItemProperty::SWEEP_ANGLE:
        return SweepAngle();
    case ItemProperty::TRIANGLE_BASE:
        return ContentSize().width();
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

void ShapeItem::SetPropertyValue(ItemProperty property, const QVariant& value)
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
    case ItemProperty::START_ANGLE:
        SetArcAngles(value.toDouble(), SweepAngle());
        break;
    case ItemProperty::SWEEP_ANGLE:
        SetArcAngles(StartAngle(), value.toDouble());
        break;
    case ItemProperty::TRIANGLE_BASE:
        SetContentSize(QSizeF(value.toDouble(), ContentSize().height()));
        break;
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

QString ShapeItem::DisplayName() const
{
    if (!m_customName.isEmpty()) {
        return m_customName;
    }
    return ShapeTypeName(shapeType());
}

QPointF ShapeItem::triangleVertex(TriangleVertex vertex) const
{
    const QRectF rect = ContentRect();
    switch (vertex) {
    case TriangleVertex::A:
        return rect.bottomLeft();
    case TriangleVertex::B:
        return rect.bottomRight();
    case TriangleVertex::C:
    default: {
        // 顶点位置与 ShapePathBuilder::trianglePath 同一套算法：高度由底边与两个底角派生。
        const qreal cot1 = 1.0 / qTan(qDegreesToRadians(m_bottomAngle1));
        const qreal cot2 = 1.0 / qTan(qDegreesToRadians(m_bottomAngle2));
        qreal apexX = rect.left() + rect.height() * cot1;
        const qreal offset = apexX - rect.left();
        const qreal sum = rect.height() * (cot1 + cot2);
        if (sum > rect.width() && sum > 0.0) {
            apexX = rect.left() + qBound<qreal>(0.0, offset * rect.width() / sum, rect.width());
        } else {
            apexX = rect.left() + qBound<qreal>(0.0, offset, rect.width());
        }
        return QPointF(apexX, rect.top());
    }
    }
}

QPointF ShapeItem::CustomHandleCenter(int handle, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (handle == CanvasItem::APEX && shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        if (ok) {
            *ok = true;
        }
        return triangleVertex(TriangleVertex::C);
    }
    return QPointF();
}

void ShapeItem::SetTriangleAngles(qreal angle1, qreal angle2)
{
    const qreal a1 = qBound<qreal>(1.0, angle1, 174.0);
    const qreal a2 = qBound<qreal>(1.0, angle2, 175.0 - a1);
    if (qFuzzyCompare(m_bottomAngle1 + 2.0, a1 + 2.0) && qFuzzyCompare(m_bottomAngle2 + 2.0, a2 + 2.0)) {
        return;
    }
    m_bottomAngle1 = a1;
    m_bottomAngle2 = a2;
    // 角度优先：底边保持当前宽度，高度按两个底角重新派生。
    const QSizeF size = ContentSize();
    const qreal cot1 = 1.0 / qTan(qDegreesToRadians(a1));
    const qreal cot2 = 1.0 / qTan(qDegreesToRadians(a2));
    const qreal height = size.width() / qMax<qreal>(0.02, cot1 + cot2);
    m_syncingTriangle = true;
    SetContentSize(QSizeF(size.width(), height));
    m_syncingTriangle = false;
    m_triangleSyncSize = ContentSize();
    update();
}

void ShapeItem::SetAngleReadoutVisible(bool visible)
{
    if (m_angleReadoutVisible == visible) {
        return;
    }
    m_angleReadoutVisible = visible;
    update();
}

void ShapeItem::SetParallelogramAngle(qreal angle)
{
    const qreal clamped = qBound<qreal>(15.0, angle, 165.0);
    if (qFuzzyCompare(m_parallelogramAngle + 2.0, clamped + 2.0)) {
        return;
    }
    m_parallelogramAngle = clamped;
    // 角度只改上边相对底边的水平偏移，内容矩形宽高不变。
    prepareGeometryChange();
    update();
}

void ShapeItem::ContentResized()
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

QSizeF ShapeItem::AdjustedContentSize(const QSizeF& size) const
{
    if (shapeType() != ShapeType::TRIANGLE || IsCustomShape()) {
        return size;
    }
    const QSizeF current = ContentSize();
    const qreal cot1 = 1.0 / qTan(qDegreesToRadians(m_bottomAngle1));
    const qreal cot2 = 1.0 / qTan(qDegreesToRadians(m_bottomAngle2));
    const qreal factor = qMax<qreal>(0.02, cot1 + cot2);
    const bool widthChanged = !qFuzzyCompare(size.width() + 0.01, current.width() + 0.01);
    const bool heightChanged = !qFuzzyCompare(size.height() + 0.01, current.height() + 0.01);
    QSizeF target = size;
    if (heightChanged && !widthChanged) {
        // 拖高等价于等比改底边：高度是本次输入，底边按角度系数放大 / 缩小。
        target.setWidth(size.height() * factor);
    } else if (widthChanged) {
        // 拖底边：高度由底边与角度派生。
        target.setHeight(size.width() / factor);
    }
    return target;
}

QRectF ShapeItem::FrameRect() const
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

void ShapeItem::DrawSelectionOverlay(QPainter* painter)
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
    painter->restore();
}

void ShapeItem::SetCustomShape(const QString& name, const QPainterPath& unitPath)
{
    m_customName = name;
    m_customPath = unitPath;
    if (!m_customPath.isEmpty()) {
        m_cornerRadius = 0.0;
    }
    update();
}

void ShapeItem::SetStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth)
{
    m_strokeColor = strokeColor.isValid() ? strokeColor : QColor(30, 41, 59);
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_strokeWidth = qBound<qreal>(0.5, strokeWidth, 48.0);
    update();
}

void ShapeItem::SetFill(const QColor& fillColor, FillStyle style)
{
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_fillStyle = style;
    update();
}

void ShapeItem::SetFillStyle(FillStyle style)
{
    if (m_fillStyle == style) {
        return;
    }
    m_fillStyle = style;
    update();
}

void ShapeItem::SetStrokeStyle(Qt::PenStyle style)
{
    if (m_strokeStyle == style) {
        return;
    }
    m_strokeStyle = style;
    update();
}

void ShapeItem::SetItemOpacity(qreal opacity)
{
    const qreal clamped = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_opacity, clamped)) {
        return;
    }
    m_opacity = clamped;
    update();
}

void ShapeItem::SetCornerRadius(qreal radius)
{
    // 圆角半径按画布像素处理，不超过短边的一半；0 表示直角。
    const qreal clamped = qBound<qreal>(0.0, radius, qMin(ContentRect().width(), ContentRect().height()) / 2.0);
    if (qFuzzyCompare(m_cornerRadius + 1.0, clamped + 1.0)) {
        return;
    }
    m_cornerRadius = clamped;
    update();
}

void ShapeItem::SetFillImage(const QPixmap& image, ImageFill::Mode mode)
{
    if (image.isNull()) {
        ClearFillImage();
        return;
    }
    m_fillImage = image;
    m_fillImageMode = mode;
    // 图片内容变了，旧的内容哈希 id 失效，由保存时按新内容重新签发。
    m_fillImageId.clear();
    update();
}

void ShapeItem::ClearFillImage()
{
    if (m_fillImage.isNull()) {
        return;
    }
    m_fillImage = QPixmap();
    m_fillImageId.clear();
    update();
}

void ShapeItem::SetArcAngles(qreal startAngle, qreal sweepAngle)
{
    const qreal start = qBound<qreal>(-360.0, startAngle, 360.0);
    const qreal sweep = qBound<qreal>(-360.0, sweepAngle, 360.0);
    if (qFuzzyCompare(m_startAngle + 2.0, start + 2.0) && qFuzzyCompare(m_sweepAngle + 2.0, sweep + 2.0)) {
        return;
    }
    m_startAngle = start;
    m_sweepAngle = sweep;
    update();
}

ShapeItemState ShapeItem::State() const
{
    ShapeItemState result;
    result.strokeColor = m_strokeColor;
    result.fillColor = m_fillColor;
    result.fillStyle = m_fillStyle;
    result.strokeWidth = m_strokeWidth;
    result.strokeStyle = m_strokeStyle;
    result.opacity = m_opacity;
    result.cornerRadius = m_cornerRadius;
    result.startAngle = m_startAngle;
    result.sweepAngle = m_sweepAngle;
    result.bottomAngle1 = m_bottomAngle1;
    result.bottomAngle2 = m_bottomAngle2;
    result.parallelogramAngle = m_parallelogramAngle;
    result.hasFillImage = HasFillImage();
    result.fillImage = m_fillImage;
    result.fillImageMode = m_fillImageMode;
    result.pos = pos();
    result.size = ContentSize();
    WriteState(result);
    return result;
}

void ShapeItem::ApplyState(const ShapeItemState& state)
{
    m_fillStyle = state.fillStyle;
    m_strokeStyle = state.strokeStyle;
    m_opacity = state.opacity;
    m_cornerRadius = state.cornerRadius;
    m_startAngle = state.startAngle;
    m_sweepAngle = state.sweepAngle;
    m_bottomAngle1 = state.bottomAngle1;
    m_bottomAngle2 = state.bottomAngle2;
    m_parallelogramAngle = qBound<qreal>(15.0, state.parallelogramAngle, 165.0);
    m_fillImage = state.hasFillImage ? state.fillImage : QPixmap();
    m_fillImageMode = state.fillImageMode;
    SetStyle(state.strokeColor, state.fillColor, state.strokeWidth);
    setPos(state.pos);
    m_syncingTriangle = true;
    SetContentSize(state.size.isEmpty() ? DefaultContentSize() : state.size);
    // 角度优先：应用状态时按角度重算三角形高度（旧尺寸与角度自洽，不会造成视觉变化）。
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        const qreal factor =
            1.0 / qTan(qDegreesToRadians(m_bottomAngle1)) + 1.0 / qTan(qDegreesToRadians(m_bottomAngle2));
        const QSizeF size = ContentSize();
        SetContentSize(QSizeF(size.width(), size.width() / qMax<qreal>(0.02, factor)));
    }
    m_syncingTriangle = false;
    ReadState(state);
    m_triangleSyncSize = ContentSize();
    update();
}

QPainterPath ShapeItem::shape() const
{
    return ShapeHitGeometry::HitShape(CurrentPath(ContentRect()), shapeType(), m_strokeWidth, SelectionHandleShape());
}

QPainterPath ShapeItem::OutlineShape() const
{
    const QPainterPath path = CurrentPath(ContentRect());
    return path.isEmpty() ? CanvasItem::OutlineShape() : path;
}

QPainterPath ShapeItem::PaintedShape() const
{
    return ShapeHitGeometry::PaintedShape(CurrentPath(ContentRect()), shapeType(), m_fillStyle, m_fillColor,
                                          m_strokeWidth, m_strokeStyle);
}

QPainterPath ShapeItem::CurrentPath(const QRectF& rect) const
{
    if (!m_customPath.isEmpty()) {
        return ShapePathBuilder::MapUnitPath(m_customPath, rect);
    }
    return ShapePath(rect);
}

QPainterPath ShapeItem::PathFor(ShapeType type, const QRectF& rect)
{
    return PathFor(type, rect, 0.0, 30.0, 120.0);
}

QPainterPath ShapeItem::PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return PathFor(type, rect, cornerRadius, 30.0, 120.0);
}

QPainterPath ShapeItem::PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle)
{
    return ShapePathBuilder::PathFor(type, rect, cornerRadius, startAngle, sweepAngle);
}

void ShapeItem::PaintContent(QPainter* painter)
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
        painter->setOpacity(painter->opacity() * m_opacity);
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
                             m_fillStyle, m_opacity);
}

ShapeLayer ShapeItem::shapeLayer(int index) const
{
    Q_UNUSED(index);
    return ShapeLayer();
}

QPainterPath ShapeItem::MapShapeLayerPath(const ShapeLayer& layer) const
{
    return layer.path;
}

bool ShapeItem::HasShapeFillArea() const
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

void ShapeItem::WriteState(ShapeItemState& state) const
{
    Q_UNUSED(state);
}

void ShapeItem::ReadState(const ShapeItemState& state)
{
    Q_UNUSED(state);
}

void ShapeItem::WriteJsonExtra(QJsonObject& json) const
{
    Q_UNUSED(json);
}

void ShapeItem::ReadJsonExtra(const QJsonObject& json)
{
    Q_UNUSED(json);
}
