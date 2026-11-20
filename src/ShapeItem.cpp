#include "ShapeItem.h"

#include "ArrowStyle.h"
#include "I18n.h"
#include "ShapeHitGeometry.h"
#include "ShapeMeta.h"
#include "ShapePainter.h"
#include "ShapePathBuilder.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QtMath>

namespace {

// 自定义图形轮廓随画布内嵌：QPainterPath 本身没有 JSON 表示，用 QDataStream
// 序列化再 base64，比手写点表更可靠，也兼容曲线与多子路径。
QString encodePathData(const QPainterPath& path)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << path;
    return QString::fromLatin1(data.toBase64());
}

QPainterPath decodePathData(const QString& text)
{
    QByteArray data = QByteArray::fromBase64(text.toLatin1());
    if (data.isEmpty()) {
        return QPainterPath();
    }
    QDataStream stream(&data, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    QPainterPath path;
    stream >> path;
    return path;
}

} // namespace

// 填充纹理的中心线。绘制与遮挡计算共用这一份几何，保证看到的和“挡住别人的”一致。
QPainterPath fillPatternCenterLines(FillStyle style, const QRectF& rect)
{
    return ShapePainter::centerLines(style, rect);
}

QList<ShapeType> allShapeTypes()
{
    return ShapeMeta::allTypes();
}

QString shapeTypeName(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return ShapeItem::tr("Rectangle");
    case ShapeType::Ellipse:
        return ShapeItem::tr("Ellipse");
    case ShapeType::Triangle:
        return ShapeItem::tr("Triangle");
    case ShapeType::Diamond:
        return ShapeItem::tr("Diamond");
    case ShapeType::Star:
        return ShapeItem::tr("Star");
    case ShapeType::Arrow:
        return ShapeItem::tr("Arrow");
    case ShapeType::DoubleArrow:
        return ShapeItem::tr("Double arrow");

    case ShapeType::Callout:
        return ShapeItem::tr("Speech bubble");
    case ShapeType::Sector:
        return ShapeItem::tr("Sector");

    case ShapeType::Star4:
        return ShapeItem::tr("Four-point star");
    case ShapeType::Hexagram:
        return ShapeItem::tr("Six-point star");
    case ShapeType::Heart:
        return ShapeItem::tr("Heart");

    case ShapeType::FlagWave:
        return ShapeItem::tr("Wavy flag");
    case ShapeType::Scroll:
        return ShapeItem::tr("Scroll");
    case ShapeType::Cube:
        return ShapeItem::tr("Cube");
    case ShapeType::Cylinder:
        return ShapeItem::tr("Cylinder");
    case ShapeType::Parallelogram:
        return ShapeItem::tr("Parallelogram");
    }
    return ShapeItem::tr("Rectangle");
}

QString shapeTypeToString(ShapeType type)
{
    return ShapeMeta::typeToString(type);
}

ShapeType shapeTypeFromString(const QString& value)
{
    return ShapeMeta::typeFromString(value);
}

QSizeF defaultShapeSize(ShapeType type)
{
    return ShapeMeta::defaultSize(type);
}

bool shapeTypeIsFilled(ShapeType type)
{
    return ShapeMeta::isFilled(type);
}


QList<FillStyle> allFillStyles()
{
    return ShapeMeta::allStyles();
}

QString fillStyleName(FillStyle style)
{
    switch (style) {
    case FillStyle::None:
        return ShapeItem::tr("No fill");
    case FillStyle::Solid:
        return ShapeItem::tr("Solid");
    case FillStyle::Hachure:
        return ShapeItem::tr("Hatch");
    case FillStyle::ReverseHachure:
        return ShapeItem::tr("Reverse hatch");
    case FillStyle::CrossHatch:
        return ShapeItem::tr("Cross hatch");
    case FillStyle::GridHatch:
        return ShapeItem::tr("Grid hatch");
    case FillStyle::Honeycomb:
        return ShapeItem::tr("Hexagon");
    case FillStyle::HorizontalHatch:
        return ShapeItem::tr("Horizontal lines");
    case FillStyle::VerticalHatch:
        return ShapeItem::tr("Vertical lines");
    case FillStyle::Brick:
        return ShapeItem::tr("Brick");
    case FillStyle::Dots:
        return ShapeItem::tr("Dots");
    }
    return ShapeItem::tr("Solid");
}

QString fillStyleToString(FillStyle style)
{
    return ShapeMeta::styleToString(style);
}

FillStyle fillStyleFromString(const QString& value)
{
    return ShapeMeta::styleFromString(value);
}

QIcon makeShapeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    return ShapePainter::makeIcon(type, color, logicalSize);
}

QIcon makeShapePresetIcon(const QString& preset, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QRectF box(3.0, 4.0, 18.0, 16.0);
    if (preset == QStringLiteral("roundedRect")) {
        // 与插入值一致：圆角取短边的 16%。
        const qreal radius = qMin(box.height(), box.width()) * 0.16;
        painter.drawRoundedRect(box, radius, radius);
    } else if (preset == QStringLiteral("triangle45")) {
        // 45° 预置：底角 90° 在左下、45° 在右下，顶点落在左上角，画成等腰直角三角形。
        const QRectF square(5.0, 5.0, 14.0, 14.0);
        QPainterPath path;
        path.moveTo(square.left(), square.top());
        path.lineTo(square.left(), square.bottom());
        path.lineTo(square.right(), square.bottom());
        path.closeSubpath();
        painter.drawPath(path);
    } else if (preset == QStringLiteral("triangle60")) {
        // 60° 预置：同一条竖边，底角 60° 让底边更短，整体更瘦高。
        const qreal height = 15.0;
        const qreal width = height / qTan(qDegreesToRadians(60.0));
        const QRectF tall(6.0, 4.5, width, height);
        QPainterPath path;
        path.moveTo(tall.left(), tall.top());
        path.lineTo(tall.left(), tall.bottom());
        path.lineTo(tall.right(), tall.bottom());
        path.closeSubpath();
        painter.drawPath(path);
    } else if (preset == QStringLiteral("braceLeft") || preset == QStringLiteral("braceRight")) {
        // 需求 13：花括号已是曲线图元，图标直接用它插入时的采样路径绘制。
        QRectF braceBox(7.0, 2.0, 10.0, 20.0);
        const QPainterPath brace = ShapePathBuilder::bracePath(braceBox);
        if (preset == QStringLiteral("braceRight")) {
            QTransform mirror;
            mirror.translate(braceBox.left() + braceBox.right(), 0.0);
            mirror.scale(-1.0, 1.0);
            painter.drawPath(mirror.map(brace));
        } else {
            painter.drawPath(brace);
        }
    } else if (preset == QStringLiteral("lineArrow")) {
        const qreal y = box.center().y();
        painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        painter.drawLine(QPointF(box.right() - 4.5, y - 4.0), QPointF(box.right(), y));
        painter.drawLine(QPointF(box.right() - 4.5, y + 4.0), QPointF(box.right(), y));
    }
    painter.end();

    return QIcon(pixmap);
}

ShapeItem::ShapeItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
}

ItemPropertySet ShapeItem::properties() const
{
    // 公共样式六件套；圆角 / 角度 / 箭头等按图形由子类自行追加。
    return CanvasItem::properties().united(ItemPropertySet{ItemProperty::FILL_STYLE, ItemProperty::FILL_COLOR,
                                                           ItemProperty::FILL_IMAGE, ItemProperty::STROKE_STYLE,
                                                           ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH});
}

QVariant ShapeItem::propertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::FILL_STYLE:
        return static_cast<int>(fillStyle());
    case ItemProperty::FILL_COLOR:
        return fillColor();
    case ItemProperty::FILL_IMAGE:
        return QVariant::fromValue(fillImage());
    case ItemProperty::STROKE_STYLE:
        return static_cast<int>(strokeStyle());
    case ItemProperty::STROKE_COLOR:
        return strokeColor();
    case ItemProperty::STROKE_WIDTH:
        return strokeWidth();
    case ItemProperty::CORNER_RADIUS:
        return cornerRadius();
    case ItemProperty::START_ANGLE:
        return startAngle();
    case ItemProperty::SWEEP_ANGLE:
        return sweepAngle();
    case ItemProperty::TRIANGLE_BASE:
        return contentSize().width();
    case ItemProperty::BOTTOM_ANGLE1:
        return bottomAngle1();
    case ItemProperty::BOTTOM_ANGLE2:
        return bottomAngle2();
    case ItemProperty::PARALLELOGRAM_ANGLE:
        return parallelogramAngle();
    default:
        return CanvasItem::propertyValue(property);
    }
}

void ShapeItem::setPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::FILL_STYLE:
        setFillStyle(static_cast<FillStyle>(value.toInt()));
        break;
    case ItemProperty::FILL_COLOR:
        setFill(value.value<QColor>(), fillStyle());
        break;
    case ItemProperty::FILL_IMAGE: {
        const QPixmap image = value.value<QPixmap>();
        if (image.isNull()) {
            clearFillImage();
        } else {
            setFillImage(image, fillImageMode());
        }
        break;
    }
    case ItemProperty::STROKE_STYLE:
        setStrokeStyle(static_cast<Qt::PenStyle>(value.toInt()));
        break;
    case ItemProperty::STROKE_COLOR:
        setStyle(value.value<QColor>(), fillColor(), strokeWidth());
        break;
    case ItemProperty::STROKE_WIDTH:
        setStyle(strokeColor(), fillColor(), value.toDouble());
        break;
    case ItemProperty::CORNER_RADIUS:
        setCornerRadius(value.toDouble());
        break;
    case ItemProperty::START_ANGLE:
        setArcAngles(value.toDouble(), sweepAngle());
        break;
    case ItemProperty::SWEEP_ANGLE:
        setArcAngles(startAngle(), value.toDouble());
        break;
    case ItemProperty::TRIANGLE_BASE:
        setContentSize(QSizeF(value.toDouble(), contentSize().height()));
        break;
    case ItemProperty::BOTTOM_ANGLE1:
        setTriangleAngles(value.toDouble(), bottomAngle2());
        break;
    case ItemProperty::BOTTOM_ANGLE2:
        setTriangleAngles(bottomAngle1(), value.toDouble());
        break;
    case ItemProperty::PARALLELOGRAM_ANGLE:
        setParallelogramAngle(value.toDouble());
        break;
    default:
        CanvasItem::setPropertyValue(property, value);
        break;
    }
}

QString ShapeItem::displayName() const
{
    if (!m_customName.isEmpty()) {
        return m_customName;
    }
    return shapeTypeName(shapeType());
}

QPointF ShapeItem::triangleVertex(TriangleVertex vertex) const
{
    const QRectF rect = contentRect();
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

QPointF ShapeItem::customHandleCenter(int handle, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (handle == CanvasItem::Apex && shapeType() == ShapeType::Triangle && !isCustomShape()) {
        if (ok) {
            *ok = true;
        }
        return triangleVertex(TriangleVertex::C);
    }
    return QPointF();
}

void ShapeItem::setTriangleAngles(qreal angle1, qreal angle2)
{
    const qreal a1 = qBound<qreal>(1.0, angle1, 174.0);
    const qreal a2 = qBound<qreal>(1.0, angle2, 175.0 - a1);
    if (qFuzzyCompare(m_bottomAngle1 + 2.0, a1 + 2.0) && qFuzzyCompare(m_bottomAngle2 + 2.0, a2 + 2.0)) {
        return;
    }
    m_bottomAngle1 = a1;
    m_bottomAngle2 = a2;
    // 角度优先：底边保持当前宽度，高度按两个底角重新派生。
    const QSizeF size = contentSize();
    const qreal cot1 = 1.0 / qTan(qDegreesToRadians(a1));
    const qreal cot2 = 1.0 / qTan(qDegreesToRadians(a2));
    const qreal height = size.width() / qMax<qreal>(0.02, cot1 + cot2);
    m_syncingTriangle = true;
    setContentSize(QSizeF(size.width(), height));
    m_syncingTriangle = false;
    m_triangleSyncSize = contentSize();
    update();
}

void ShapeItem::setAngleReadoutVisible(bool visible)
{
    if (m_angleReadoutVisible == visible) {
        return;
    }
    m_angleReadoutVisible = visible;
    update();
}

void ShapeItem::setParallelogramAngle(qreal angle)
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

void ShapeItem::contentResized()
{
    if (shapeType() != ShapeType::Triangle || isCustomShape()) {
        return;
    }
    // 尺寸折算已经在 adjustedContentSize() 里做完，这里只记一次同步基准，
    // 不再在 resize 过程中二次改尺寸，避免拖动底边时反复重排导致闪烁。
    if (!m_syncingTriangle) {
        m_triangleSyncSize = contentSize();
    }
}

QSizeF ShapeItem::adjustedContentSize(const QSizeF& size) const
{
    if (shapeType() != ShapeType::Triangle || isCustomShape()) {
        return size;
    }
    const QSizeF current = contentSize();
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

QRectF ShapeItem::frameRect() const
{
    // 扇形只画了内容矩形里的一小块，选中框与手柄改贴在轮廓的最小覆盖矩形上。
    if (m_customPath.isEmpty() && shapeType() == ShapeType::Sector) {
        const QPainterPath path = currentPath(contentRect());
        const QRectF bounds = path.boundingRect();
        if (!bounds.isEmpty()) {
            return bounds;
        }
    }
    return CanvasItem::frameRect();
}

void ShapeItem::drawSelectionOverlay(QPainter* painter)
{
    CanvasItem::drawSelectionOverlay(painter);
    if (shapeType() != ShapeType::Triangle || isCustomShape() || !isSelected() || !isEditable() ||
        !hasSingleSelection()) {
        return;
    }
    // 底边中点的底边长度手柄两侧各补一根短刻度线，提示这里拖的是底边。
    const QRectF frame = frameRect();
    const qreal gap = qMax<qreal>(4.0, 6.0 / qMax<qreal>(viewScale(), 0.001));
    const qreal tick = qMax<qreal>(4.0, 7.0 / qMax<qreal>(viewScale(), 0.001));
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

void ShapeItem::setCustomShape(const QString& name, const QPainterPath& unitPath)
{
    m_customName = name;
    m_customPath = unitPath;
    if (!m_customPath.isEmpty()) {
        m_cornerRadius = 0.0;
    }
    update();
}

void ShapeItem::setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth)
{
    m_strokeColor = strokeColor.isValid() ? strokeColor : QColor(30, 41, 59);
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_strokeWidth = qBound<qreal>(0.5, strokeWidth, 48.0);
    update();
}

void ShapeItem::setFill(const QColor& fillColor, FillStyle style)
{
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_fillStyle = style;
    update();
}

void ShapeItem::setFillStyle(FillStyle style)
{
    if (m_fillStyle == style) {
        return;
    }
    m_fillStyle = style;
    update();
}

void ShapeItem::setStrokeStyle(Qt::PenStyle style)
{
    if (m_strokeStyle == style) {
        return;
    }
    m_strokeStyle = style;
    update();
}

void ShapeItem::setItemOpacity(qreal opacity)
{
    const qreal clamped = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_opacity, clamped)) {
        return;
    }
    m_opacity = clamped;
    update();
}

void ShapeItem::setCornerRadius(qreal radius)
{
    // 圆角半径按画布像素处理，不超过短边的一半；0 表示直角。
    const qreal clamped = qBound<qreal>(0.0, radius, qMin(contentRect().width(), contentRect().height()) / 2.0);
    if (qFuzzyCompare(m_cornerRadius + 1.0, clamped + 1.0)) {
        return;
    }
    m_cornerRadius = clamped;
    update();
}

void ShapeItem::setFillImage(const QPixmap& image, ImageFill::Mode mode)
{
    if (image.isNull()) {
        clearFillImage();
        return;
    }
    m_fillImage = image;
    m_fillImageMode = mode;
    // 图片内容变了，旧的内容哈希 id 失效，由保存时按新内容重新签发。
    m_fillImageId.clear();
    update();
}

void ShapeItem::clearFillImage()
{
    if (m_fillImage.isNull()) {
        return;
    }
    m_fillImage = QPixmap();
    m_fillImageId.clear();
    update();
}

void ShapeItem::setArcAngles(qreal startAngle, qreal sweepAngle)
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

ShapeItemState ShapeItem::state() const
{
    ShapeItemState result;
    result.type = shapeType();
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
    result.hasFillImage = hasFillImage();
    result.fillImage = m_fillImage;
    result.fillImageMode = m_fillImageMode;
    result.pos = pos();
    result.size = contentSize();
    result.polygon = isPolygon();
    writeState(result);
    return result;
}

void ShapeItem::applyState(const ShapeItemState& state)
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
    setStyle(state.strokeColor, state.fillColor, state.strokeWidth);
    setPos(state.pos);
    m_syncingTriangle = true;
    setContentSize(state.size.isEmpty() ? defaultShapeSize(shapeType()) : state.size);
    // 角度优先：应用状态时按角度重算三角形高度（旧尺寸与角度自洽，不会造成视觉变化）。
    if (shapeType() == ShapeType::Triangle && !isCustomShape()) {
        const qreal factor =
            1.0 / qTan(qDegreesToRadians(m_bottomAngle1)) + 1.0 / qTan(qDegreesToRadians(m_bottomAngle2));
        const QSizeF size = contentSize();
        setContentSize(QSizeF(size.width(), size.width() / qMax<qreal>(0.02, factor)));
    }
    m_syncingTriangle = false;
    readState(state);
    m_triangleSyncSize = contentSize();
    update();
}

QPainterPath ShapeItem::shape() const
{
    return ShapeHitGeometry::hitShape(currentPath(contentRect()), shapeType(), m_strokeWidth, selectionHandleShape());
}

QPainterPath ShapeItem::outlineShape() const
{
    const QPainterPath path = currentPath(contentRect());
    return path.isEmpty() ? CanvasItem::outlineShape() : path;
}

QPainterPath ShapeItem::paintedShape() const
{
    return ShapeHitGeometry::paintedShape(currentPath(contentRect()), shapeType(), m_fillStyle, m_fillColor,
                                          m_strokeWidth, m_strokeStyle);
}

QPainterPath ShapeItem::currentPath(const QRectF& rect) const
{
    if (!m_customPath.isEmpty()) {
        return ShapePathBuilder::mapUnitPath(m_customPath, rect);
    }
    return shapePath(rect);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect)
{
    return pathFor(type, rect, 0.0, 30.0, 120.0);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return pathFor(type, rect, cornerRadius, 30.0, 120.0);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle)
{
    return ShapePathBuilder::pathFor(type, rect, cornerRadius, startAngle, sweepAngle);
}

void ShapeItem::paintContent(QPainter* painter)
{
    const QRectF rect = contentRect();
    if (rect.isEmpty()) {
        return;
    }

    const QPainterPath path = currentPath(rect);
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
        ImageFill::paint(painter, m_fillImage, m_fillImageMode, rect, clip);
        painter->restore();
    }

    ShapePainter::paintShape(painter, path, rect, shapeType(), m_strokeColor, m_fillColor, m_strokeWidth, m_strokeStyle,
                             m_fillStyle, m_opacity);
}

QJsonObject ShapeItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("shapeType"), shapeTypeToString(shapeType()));
    json.insert(QStringLiteral("strokeColor"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillColor"), m_fillColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillStyle"), fillStyleToString(m_fillStyle));
    json.insert(QStringLiteral("strokeWidth"), m_strokeWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("opacity"), m_opacity);
    json.insert(QStringLiteral("cornerRadius"), m_cornerRadius);
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
    // 三角形以角度为真相，两个底角随画布保存；旧文件没有这两个字段时按 60°/60° 反算。
    if (shapeType() == ShapeType::Triangle) {
        json.insert(QStringLiteral("bottomAngle1"), m_bottomAngle1);
        json.insert(QStringLiteral("bottomAngle2"), m_bottomAngle2);
    }
    // 需求 9：平行四边形的左下内角随画布保存；旧文件缺字段时按 60° 载入。
    if (shapeType() == ShapeType::Parallelogram) {
        json.insert(QStringLiteral("parallelogramAngle"), m_parallelogramAngle);
    }
    // 需求 15：图片填充独立属性，无图时不写键，旧版本忽略该键即退回纯色。
    // 需求 11：有内容哈希 id 时只写引用（与图片模式），图片本体由画布级 images 表承载；
    // 没有 id（模板 / 剪贴板这类自包含文件）仍写内联 data。
    if (!m_fillImage.isNull() && !m_fillImageId.isEmpty()) {
        json.insert(QStringLiteral("fillImageId"), m_fillImageId);
        json.insert(QStringLiteral("fillImageMode"), ImageFill::modeToString(m_fillImageMode));
    } else {
        const QJsonObject fillImageJson = ImageFill::toJson(m_fillImage, m_fillImageMode);
        if (!fillImageJson.isEmpty()) {
            json.insert(QStringLiteral("fillImage"), fillImageJson);
        }
    }
    // 需求 30：自定义轮廓不再一律写成 "custom"（旧版读回会退化成矩形），真实
    // 类型照常写进 shapeType；名字与轮廓单独存，只有名字的图元（立方体顶面）
    // 也能在重开后保留名称。
    if (!m_customName.isEmpty()) {
        json.insert(QStringLiteral("customName"), m_customName);
    }
    if (!m_customPath.isEmpty()) {
        json.insert(QStringLiteral("pathData"), encodePathData(m_customPath));
    }
    writeJsonExtra(json);
    return json;
}

void ShapeItem::fromJson(const QJsonObject& json)
{
    const QString typeName = json.value(QStringLiteral("shapeType")).toString();
    bool forceRoundedCorner = false;
    if (typeName == QStringLiteral("custom")) {
        m_customName = json.value(QStringLiteral("customName")).toString();
        m_customPath = decodePathData(json.value(QStringLiteral("pathData")).toString());
    } else {
        // 需求 5：旧的 rounded_rectangle 读成「矩形 + 自动圆角」，不向后写兼容。
        const QString normalizedType = typeName.trimmed().toLower();
        const bool legacyRounded = normalizedType == QStringLiteral("rounded_rectangle") ||
                                   normalizedType == QStringLiteral("roundedrect") ||
                                   normalizedType == QStringLiteral("roundrect");
        m_customName.clear();
        m_customPath = QPainterPath();
        if (legacyRounded) {
            forceRoundedCorner = true;
        }
        // 需求 30：新文件把自定义轮廓挂在真实类型上（立方体的菱形面），
        // 旧文件仍是 "custom" + 矩形底层。
        const QString customName = json.value(QStringLiteral("customName")).toString();
        if (!customName.isEmpty()) {
            m_customName = customName;
        }
        const QString pathData = json.value(QStringLiteral("pathData")).toString();
        if (!pathData.isEmpty()) {
            m_customPath = decodePathData(pathData);
        }
    }

    m_strokeColor = QColor(json.value(QStringLiteral("strokeColor")).toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid()) {
        m_strokeColor = QColor(30, 41, 59);
    }

    if (json.contains(QStringLiteral("fillColor"))) {
        m_fillColor = QColor(json.value(QStringLiteral("fillColor")).toString());
        if (!m_fillColor.isValid()) {
            m_fillColor = QColor(Qt::transparent);
        }
    } else {
        m_fillColor = m_strokeColor;
        m_fillColor.setAlpha(36);
    }

    if (json.contains(QStringLiteral("fillStyle"))) {
        m_fillStyle = fillStyleFromString(json.value(QStringLiteral("fillStyle")).toString());
    } else {
        m_fillStyle = m_fillColor.alpha() > 0 ? FillStyle::Solid : FillStyle::None;
    }

    m_strokeWidth = qBound<qreal>(0.5, json.value(QStringLiteral("strokeWidth")).toDouble(4.0), 48.0);
    m_strokeStyle = static_cast<Qt::PenStyle>(qBound(0, json.value(QStringLiteral("strokeStyle")).toInt(1), 5));
    m_opacity = qBound<qreal>(0.0, json.value(QStringLiteral("opacity")).toDouble(1.0), 1.0);
    m_cornerRadius = qMax<qreal>(0.0, json.value(QStringLiteral("cornerRadius")).toDouble(0.0));
    // 旧文件的 cornerAuto / 负圆角按「短边 16%」折算成显式 px；新文件只存 px。
    const bool legacyAutoCorner = json.value(QStringLiteral("cornerAuto")).toBool(false) ||
                                  json.value(QStringLiteral("cornerRadius")).toDouble(0.0) < 0.0;
    m_startAngle = json.value(QStringLiteral("startAngle")).toDouble(30.0);
    m_sweepAngle = json.value(QStringLiteral("sweepAngle")).toDouble(120.0);
    // 需求 9：平行四边形内角；旧文件没有这个键时按默认 60°。
    m_parallelogramAngle = qBound<qreal>(15.0, json.value(QStringLiteral("parallelogramAngle")).toDouble(60.0), 165.0);
    // 需求 6：旧文件可能还带 shapeRadius；先记下原值，等尺寸恢复后再折算（见下方迁移）。
    const qreal legacyRadius = json.value(QStringLiteral("shapeRadius")).toDouble(-1.0);
    if (json.contains(QStringLiteral("fillImage"))) {
        m_fillImage = ImageFill::pixmapFromJson(json.value(QStringLiteral("fillImage")).toObject(), &m_fillImageMode);
        m_fillImageId.clear();
    } else if (json.contains(QStringLiteral("fillImageId"))) {
        // 需求 11：图片本体在画布级 images 表里，先记下 id，由加载层解析成 pixmap。
        m_fillImageId = json.value(QStringLiteral("fillImageId")).toString();
        m_fillImageMode = ImageFill::modeFromString(json.value(QStringLiteral("fillImageMode")).toString());
        m_fillImage = QPixmap();
    } else {
        m_fillImage = QPixmap();
        m_fillImageId.clear();
        m_fillImageMode = ImageFill::Mode::Stretch;
    }
    const bool hasAngles = json.contains(QStringLiteral("bottomAngle1"));

    // 载入期间先屏蔽三角形尺寸联动，避免 baseJson 恢复尺寸时反推高度、改动旧文件外观。
    m_syncingTriangle = true;
    applyBaseJson(json);
    if (contentSize().isEmpty()) {
        setContentSize(defaultShapeSize(shapeType()));
    }
    m_syncingTriangle = false;

    // 需求 6：扇形 / 弧线旧半径 < 内容宽的一半时，把椭圆外接框改成新内容矩形：
    // k = 2r、ry = r·h/w，位置按椭圆中心不动补偿，轮廓逐点不变；r<0 或 r≥w/2 无需迁移。
    if (shapeType() == ShapeType::Sector && legacyRadius >= 0.0 && !isCustomShape()) {
        const QSizeF size = contentSize();
        if (size.width() > 0.0 && legacyRadius < size.width() / 2.0) {
            const qreal ry = legacyRadius * size.height() / size.width();
            setContentSize(QSizeF(legacyRadius * 2.0, ry * 2.0));
            setPos(pos() + QPointF(size.width() / 2.0 - legacyRadius, size.height() / 2.0 - ry));
        }
    }

    // 旧文件没有角度字段：按原三角形的顶点位置（内容矩形上边中点）反算两个底角，
    // 使角度与恢复后的尺寸自洽，外观零变化。
    if (shapeType() == ShapeType::Triangle && !isCustomShape()) {
        if (hasAngles) {
            m_bottomAngle1 = qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle1")).toDouble(60.0), 174.0);
            m_bottomAngle2 =
                qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle2")).toDouble(60.0), 175.0 - m_bottomAngle1);
        } else {
            const QSizeF size = contentSize();
            const qreal halfWidth = qMax<qreal>(1.0, size.width() / 2.0);
            const qreal legacyAngle = qRadiansToDegrees(qAtan2(qMax<qreal>(1.0, size.height()), halfWidth));
            m_bottomAngle1 = qBound<qreal>(1.0, legacyAngle, 174.0);
            m_bottomAngle2 = qBound<qreal>(1.0, legacyAngle, 175.0 - m_bottomAngle1);
        }
    }

    readJsonExtra(json);
    if ((forceRoundedCorner || legacyAutoCorner) && !isCustomShape()) {
        m_cornerRadius = qMin(contentSize().width(), contentSize().height()) * 0.16;
    }
    m_triangleSyncSize = contentSize();
    update();
}

ShapeLayer ShapeItem::shapeLayer(int index) const
{
    Q_UNUSED(index);
    return ShapeLayer();
}

QPainterPath ShapeItem::mapShapeLayerPath(const ShapeLayer& layer) const
{
    return layer.path;
}

bool ShapeItem::hasShapeFillArea() const
{
    if (shapeLayerCount() == 0) {
        return !isPolygon();
    }
    for (int index = 0; index < shapeLayerCount(); ++index) {
        const ShapeLayer layer = shapeLayer(index);
        if (layer.role == ShapeLayerRole::FILL_AREA && !layer.path.isEmpty()) {
            return true;
        }
    }
    return false;
}

void ShapeItem::writeState(ShapeItemState& state) const
{
    Q_UNUSED(state);
}

void ShapeItem::readState(const ShapeItemState& state)
{
    Q_UNUSED(state);
}

void ShapeItem::writeJsonExtra(QJsonObject& json) const
{
    Q_UNUSED(json);
}

void ShapeItem::readJsonExtra(const QJsonObject& json)
{
    Q_UNUSED(json);
}
