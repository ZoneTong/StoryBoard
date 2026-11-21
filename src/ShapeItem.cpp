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
QPainterPath FillPatternCenterLines(FillStyle style, const QRectF& rect)
{
    return ShapePainter::CenterLines(style, rect);
}

QList<ShapeType> AllShapeTypes()
{
    return ShapeMeta::AllTypes();
}

QString ShapeTypeName(ShapeType type)
{
    switch (type) {
    case ShapeType::RECTANGLE:
        return ShapeItem::tr("Rectangle");
    case ShapeType::ELLIPSE:
        return ShapeItem::tr("Ellipse");
    case ShapeType::TRIANGLE:
        return ShapeItem::tr("Triangle");
    case ShapeType::STAR:
        return ShapeItem::tr("Star");
    case ShapeType::ARROW:
        return ShapeItem::tr("Arrow");
    case ShapeType::DOUBLE_ARROW:
        return ShapeItem::tr("Double arrow");

    case ShapeType::CALLOUT:
        return ShapeItem::tr("Speech bubble");
    case ShapeType::SECTOR:
        return ShapeItem::tr("Sector");

    case ShapeType::STAR4:
        return ShapeItem::tr("Four-point star");
    case ShapeType::HEXAGRAM:
        return ShapeItem::tr("Six-point star");
    case ShapeType::HEART:
        return ShapeItem::tr("Heart");

    case ShapeType::FLAG_WAVE:
        return ShapeItem::tr("Wavy flag");
    case ShapeType::SCROLL:
        return ShapeItem::tr("Scroll");
    case ShapeType::CUBE:
        return ShapeItem::tr("Cube");
    case ShapeType::CYLINDER:
        return ShapeItem::tr("Cylinder");
    case ShapeType::PARALLELOGRAM:
        return ShapeItem::tr("Parallelogram");
    }
    return ShapeItem::tr("Rectangle");
}

QString ShapeTypeToString(ShapeType type)
{
    return ShapeMeta::TypeToString(type);
}

ShapeType ShapeTypeFromString(const QString& value)
{
    return ShapeMeta::TypeFromString(value);
}

QSizeF DefaultShapeSize(ShapeType type)
{
    // 单一来源在子类 DefaultContentSize()：按需构造原型图元取默认尺寸，
    // 构造函数走 SetContentSize(DefaultContentSize())，不会递归。
    ShapeItem* prototype = CreateShapeItem(type);
    const QSizeF size = prototype->DefaultContentSize();
    delete prototype;
    return size;
}

bool ShapeTypeIsFilled(ShapeType type)
{
    return ShapeMeta::IsFilled(type);
}


QList<FillStyle> AllFillStyles()
{
    return ShapeMeta::AllStyles();
}

QString FillStyleName(FillStyle style)
{
    switch (style) {
    case FillStyle::NONE:
        return ShapeItem::tr("No fill");
    case FillStyle::SOLID:
        return ShapeItem::tr("Solid");
    case FillStyle::HACHURE:
        return ShapeItem::tr("Hatch");
    case FillStyle::REVERSE_HACHURE:
        return ShapeItem::tr("Reverse hatch");
    case FillStyle::CROSS_HATCH:
        return ShapeItem::tr("Cross hatch");
    case FillStyle::GRID_HATCH:
        return ShapeItem::tr("Grid hatch");
    case FillStyle::HONEYCOMB:
        return ShapeItem::tr("Hexagon");
    case FillStyle::HORIZONTAL_HATCH:
        return ShapeItem::tr("Horizontal lines");
    case FillStyle::VERTICAL_HATCH:
        return ShapeItem::tr("Vertical lines");
    case FillStyle::BRICK:
        return ShapeItem::tr("Brick");
    case FillStyle::DOTS:
        return ShapeItem::tr("Dots");
    }
    return ShapeItem::tr("Solid");
}

QString FillStyleToString(FillStyle style)
{
    return ShapeMeta::StyleToString(style);
}

FillStyle FillStyleFromString(const QString& value)
{
    return ShapeMeta::StyleFromString(value);
}

QIcon MakeShapeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    return ShapePainter::MakeIcon(type, color, logicalSize);
}

QIcon MakeShapePresetIcon(const QString& preset, const QColor& color, int logicalSize)
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
        const QPainterPath brace = ShapePathBuilder::BracePath(braceBox);
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
    } else if (preset == QStringLiteral("diamond")) {
        // 菱形预置：平行四边形 90° + 旋转 45°，外接对角线撑满图标框。
        QPainterPath path;
        path.moveTo(box.center().x(), box.top());
        path.lineTo(box.right(), box.center().y());
        path.lineTo(box.center().x(), box.bottom());
        path.lineTo(box.left(), box.center().y());
        path.closeSubpath();
        painter.drawPath(path);
    }
    painter.end();

    return QIcon(pixmap);
}

ShapeItem::ShapeItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
}

ItemPropertySet ShapeItem::Properties() const
{
    // 公共样式六件套；圆角 / 角度 / 箭头等按图形由子类自行追加。
    return CanvasItem::Properties().United(ItemPropertySet{ItemProperty::FILL_STYLE, ItemProperty::FILL_COLOR,
                                                           ItemProperty::FILL_IMAGE, ItemProperty::STROKE_STYLE,
                                                           ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH});
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

QJsonObject ShapeItem::ToJson() const
{
    QJsonObject json = BaseJson();
    json.insert(QStringLiteral("shapeType"), ShapeTypeToString(shapeType()));
    json.insert(QStringLiteral("strokeColor"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillColor"), m_fillColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillStyle"), FillStyleToString(m_fillStyle));
    json.insert(QStringLiteral("strokeWidth"), m_strokeWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("opacity"), m_opacity);
    json.insert(QStringLiteral("cornerRadius"), m_cornerRadius);
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
    // 三角形以角度为真相，两个底角随画布保存；旧文件没有这两个字段时按 60°/60° 反算。
    if (shapeType() == ShapeType::TRIANGLE) {
        json.insert(QStringLiteral("bottomAngle1"), m_bottomAngle1);
        json.insert(QStringLiteral("bottomAngle2"), m_bottomAngle2);
    }
    // 需求 9：平行四边形的左下内角随画布保存；旧文件缺字段时按 60° 载入。
    if (shapeType() == ShapeType::PARALLELOGRAM) {
        json.insert(QStringLiteral("parallelogramAngle"), m_parallelogramAngle);
    }
    // 需求 15：图片填充独立属性，无图时不写键，旧版本忽略该键即退回纯色。
    // 需求 11：有内容哈希 id 时只写引用（与图片模式），图片本体由画布级 images 表承载；
    // 没有 id（模板 / 剪贴板这类自包含文件）仍写内联 data。
    if (!m_fillImage.isNull() && !m_fillImageId.isEmpty()) {
        json.insert(QStringLiteral("fillImageId"), m_fillImageId);
        json.insert(QStringLiteral("fillImageMode"), ImageFill::ModeToString(m_fillImageMode));
    } else {
        const QJsonObject fillImageJson = ImageFill::ToJson(m_fillImage, m_fillImageMode);
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
    WriteJsonExtra(json);
    return json;
}

void ShapeItem::FromJson(const QJsonObject& json)
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
        m_fillStyle = FillStyleFromString(json.value(QStringLiteral("fillStyle")).toString());
    } else {
        m_fillStyle = m_fillColor.alpha() > 0 ? FillStyle::SOLID : FillStyle::NONE;
    }

    m_strokeWidth = qBound<qreal>(0.5, json.value(QStringLiteral("strokeWidth")).toDouble(4.0), 48.0);
    m_strokeStyle = static_cast<Qt::PenStyle>(qBound(0, json.value(QStringLiteral("strokeStyle")).toInt(1), 5));
    m_opacity = qBound<qreal>(0.0, json.value(QStringLiteral("opacity")).toDouble(1.0), 1.0);
    m_cornerRadius = qMax<qreal>(0.0, json.value(QStringLiteral("cornerRadius")).toDouble(0.0));
    m_startAngle = json.value(QStringLiteral("startAngle")).toDouble(30.0);
    m_sweepAngle = json.value(QStringLiteral("sweepAngle")).toDouble(120.0);
    // 需求 9：平行四边形内角；旧文件没有这个键时按默认 60°。
    m_parallelogramAngle = qBound<qreal>(15.0, json.value(QStringLiteral("parallelogramAngle")).toDouble(60.0), 165.0);
    // 需求 6：旧文件可能还带 shapeRadius；先记下原值，等尺寸恢复后再折算（见下方迁移）。
    const qreal legacyRadius = json.value(QStringLiteral("shapeRadius")).toDouble(-1.0);
    if (json.contains(QStringLiteral("fillImage"))) {
        m_fillImage = ImageFill::PixmapFromJson(json.value(QStringLiteral("fillImage")).toObject(), &m_fillImageMode);
        m_fillImageId.clear();
    } else if (json.contains(QStringLiteral("fillImageId"))) {
        // 需求 11：图片本体在画布级 images 表里，先记下 id，由加载层解析成 pixmap。
        m_fillImageId = json.value(QStringLiteral("fillImageId")).toString();
        m_fillImageMode = ImageFill::ModeFromString(json.value(QStringLiteral("fillImageMode")).toString());
        m_fillImage = QPixmap();
    } else {
        m_fillImage = QPixmap();
        m_fillImageId.clear();
        m_fillImageMode = ImageFill::Mode::STRETCH;
    }
    const bool hasAngles = json.contains(QStringLiteral("bottomAngle1"));

    // 载入期间先屏蔽三角形尺寸联动，避免 baseJson 恢复尺寸时反推高度、改动旧文件外观。
    m_syncingTriangle = true;
    ApplyBaseJson(json);
    if (ContentSize().isEmpty()) {
        SetContentSize(DefaultContentSize());
    }
    m_syncingTriangle = false;

    // 需求 6：扇形 / 弧线旧半径 < 内容宽的一半时，把椭圆外接框改成新内容矩形：
    // k = 2r、ry = r·h/w，位置按椭圆中心不动补偿，轮廓逐点不变；r<0 或 r≥w/2 无需迁移。
    if (shapeType() == ShapeType::SECTOR && legacyRadius >= 0.0 && !IsCustomShape()) {
        const QSizeF size = ContentSize();
        if (size.width() > 0.0 && legacyRadius < size.width() / 2.0) {
            const qreal ry = legacyRadius * size.height() / size.width();
            SetContentSize(QSizeF(legacyRadius * 2.0, ry * 2.0));
            setPos(pos() + QPointF(size.width() / 2.0 - legacyRadius, size.height() / 2.0 - ry));
        }
    }

    // 旧文件没有角度字段：按原三角形的顶点位置（内容矩形上边中点）反算两个底角，
    // 使角度与恢复后的尺寸自洽，外观零变化。
    if (shapeType() == ShapeType::TRIANGLE && !IsCustomShape()) {
        if (hasAngles) {
            m_bottomAngle1 = qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle1")).toDouble(60.0), 174.0);
            m_bottomAngle2 =
                qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle2")).toDouble(60.0), 175.0 - m_bottomAngle1);
        } else {
            const QSizeF size = ContentSize();
            const qreal halfWidth = qMax<qreal>(1.0, size.width() / 2.0);
            const qreal legacyAngle = qRadiansToDegrees(qAtan2(qMax<qreal>(1.0, size.height()), halfWidth));
            m_bottomAngle1 = qBound<qreal>(1.0, legacyAngle, 174.0);
            m_bottomAngle2 = qBound<qreal>(1.0, legacyAngle, 175.0 - m_bottomAngle1);
        }
    }

    ReadJsonExtra(json);
    if (forceRoundedCorner && !IsCustomShape()) {
        m_cornerRadius = qMin(ContentSize().width(), ContentSize().height()) * 0.16;
    }
    m_triangleSyncSize = ContentSize();
    update();
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
