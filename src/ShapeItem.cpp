#include "ShapeItem.h"

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
    case ShapeType::RoundedRectangle:
        return ShapeItem::tr("Rounded rectangle");
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
    case ShapeType::Line:
        return ShapeItem::tr("Line");
    case ShapeType::BraceLeft:
        return ShapeItem::tr("Left brace");
    case ShapeType::BraceRight:
        return ShapeItem::tr("Right brace");
    case ShapeType::Callout:
        return ShapeItem::tr("Speech bubble");
    case ShapeType::Sector:
        return ShapeItem::tr("Sector");
    case ShapeType::Arc:
        return ShapeItem::tr("Arc");
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

bool shapeTypeIsStrokeOnly(ShapeType type)
{
    return ShapeMeta::isStrokeOnly(type);
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

ShapeItem::ShapeItem(ShapeType type, QGraphicsItem* parent) : CanvasItem(parent), m_type(type)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
    setContentSize(defaultShapeSize(type));
}

QString ShapeItem::displayName() const
{
    if (!m_customName.isEmpty()) {
        return m_customName;
    }
    return shapeTypeName(m_type);
}

void ShapeItem::setShapeType(ShapeType type)
{
    if (m_type == type && m_customPath.isEmpty()) {
        return;
    }
    m_type = type;
    // 切换到内置图形时丢掉自定义轮廓，否则自定义路径会继续盖住新类型。
    m_customPath = QPainterPath();
    m_customName.clear();
    m_cornerRadius = -1.0;
    m_startAngle = 30.0;
    m_sweepAngle = 120.0;
    m_shapeRadius = -1.0;
    update();
}

void ShapeItem::setCustomShape(const QString& name, const QPainterPath& unitPath)
{
    m_customName = name;
    m_customPath = unitPath;
    if (!m_customPath.isEmpty()) {
        m_cornerRadius = -1.0;
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

void ShapeItem::setOpacity(qreal opacity)
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
    // 负数表示恢复该类型的默认圆角；正数按画布像素处理，不超过短边的一半。
    const qreal clamped =
        radius < 0.0 ? -1.0 : qBound<qreal>(0.0, radius, qMin(contentRect().width(), contentRect().height()) / 2.0);
    if (qFuzzyCompare(m_cornerRadius + 2.0, clamped + 2.0)) {
        return;
    }
    m_cornerRadius = clamped;
    update();
}

void ShapeItem::setShapeRadius(qreal radius)
{
    const qreal clamped = radius < 0.0 ? -1.0 : qMax<qreal>(0.0, radius);
    if (qFuzzyCompare(m_shapeRadius + 2.0, clamped + 2.0)) {
        return;
    }
    m_shapeRadius = clamped;
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
    result.type = m_type;
    result.strokeColor = m_strokeColor;
    result.fillColor = m_fillColor;
    result.fillStyle = m_fillStyle;
    result.strokeWidth = m_strokeWidth;
    result.strokeStyle = m_strokeStyle;
    result.opacity = m_opacity;
    result.cornerRadius = m_cornerRadius;
    result.startAngle = m_startAngle;
    result.sweepAngle = m_sweepAngle;
    result.shapeRadius = m_shapeRadius;
    result.pos = pos();
    result.size = contentSize();
    return result;
}

void ShapeItem::applyState(const ShapeItemState& state)
{
    m_type = state.type;
    m_fillStyle = state.fillStyle;
    m_strokeStyle = state.strokeStyle;
    m_opacity = state.opacity;
    m_cornerRadius = state.cornerRadius;
    m_startAngle = state.startAngle;
    m_sweepAngle = state.sweepAngle;
    m_shapeRadius = state.shapeRadius;
    setStyle(state.strokeColor, state.fillColor, state.strokeWidth);
    setPos(state.pos);
    setContentSize(state.size.isEmpty() ? defaultShapeSize(state.type) : state.size);
    update();
}

QPainterPath ShapeItem::shape() const
{
    return ShapeHitGeometry::hitShape(currentPath(contentRect()), m_type, m_strokeWidth, selectionHandleShape());
}

QPainterPath ShapeItem::outlineShape() const
{
    const QPainterPath path = currentPath(contentRect());
    return path.isEmpty() ? CanvasItem::outlineShape() : path;
}

QPainterPath ShapeItem::paintedShape() const
{
    return ShapeHitGeometry::paintedShape(currentPath(contentRect()), m_type, m_fillStyle, m_fillColor, m_strokeWidth);
}

QPainterPath ShapeItem::currentPath(const QRectF& rect) const
{
    if (!m_customPath.isEmpty()) {
        return ShapePathBuilder::mapUnitPath(m_customPath, rect);
    }
    return pathFor(m_type, rect, m_cornerRadius, m_startAngle, m_sweepAngle, m_shapeRadius);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect)
{
    return pathFor(type, rect, -1.0, 30.0, 120.0, -1.0);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius)
{
    return pathFor(type, rect, cornerRadius, 30.0, 120.0, -1.0);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle, qreal shapeRadius)
{
    return ShapePathBuilder::pathFor(type, rect, cornerRadius, startAngle, sweepAngle, shapeRadius);
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

    ShapePainter::paintShape(painter, path, rect, m_type, m_strokeColor, m_fillColor, m_strokeWidth, m_strokeStyle,
                             m_fillStyle, m_opacity);
}

QJsonObject ShapeItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("shapeType"), shapeTypeToString(m_type));
    json.insert(QStringLiteral("strokeColor"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillColor"), m_fillColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillStyle"), fillStyleToString(m_fillStyle));
    json.insert(QStringLiteral("strokeWidth"), m_strokeWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("opacity"), m_opacity);
    json.insert(QStringLiteral("cornerRadius"), m_cornerRadius);
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
    json.insert(QStringLiteral("shapeRadius"), m_shapeRadius);
    if (!m_customPath.isEmpty()) {
        json.insert(QStringLiteral("shapeType"), QStringLiteral("custom"));
        json.insert(QStringLiteral("customName"), m_customName);
        json.insert(QStringLiteral("pathData"), encodePathData(m_customPath));
    }
    return json;
}

void ShapeItem::fromJson(const QJsonObject& json)
{
    const QString typeName = json.value(QStringLiteral("shapeType")).toString();
    if (typeName == QStringLiteral("custom")) {
        m_customName = json.value(QStringLiteral("customName")).toString();
        m_customPath = decodePathData(json.value(QStringLiteral("pathData")).toString());
        m_type = ShapeType::Rectangle;
    } else {
        m_type = shapeTypeFromString(typeName);
        m_customName.clear();
        m_customPath = QPainterPath();
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
    m_cornerRadius = json.value(QStringLiteral("cornerRadius")).toDouble(-1.0);
    m_startAngle = json.value(QStringLiteral("startAngle")).toDouble(30.0);
    m_sweepAngle = json.value(QStringLiteral("sweepAngle")).toDouble(120.0);
    m_shapeRadius = json.value(QStringLiteral("shapeRadius")).toDouble(-1.0);

    applyBaseJson(json);
    if (contentSize().isEmpty()) {
        setContentSize(defaultShapeSize(m_type));
    }
    update();
}
