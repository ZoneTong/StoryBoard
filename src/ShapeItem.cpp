#include "ShapeItem.h"

#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QTransform>
#include <QtMath>

namespace {

QRectF normalizedRect(const QRectF& rect)
{
    return rect.normalized();
}

QPainterPath bracePath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0)
        return QPainterPath();

    const qreal xOuter = r.left() + w * 0.82;
    const qreal xMid = r.left() + w * 0.60;
    const qreal xTip = r.left() + w * 0.16;

    QPainterPath path;
    path.moveTo(xOuter, r.top());
    path.cubicTo(xMid, r.top() + h * 0.025,
                 xMid, r.top() + h * 0.14,
                 xMid, r.top() + h * 0.30);
    path.cubicTo(xMid, r.top() + h * 0.43,
                 xTip, r.top() + h * 0.42,
                 xTip, r.center().y());
    path.cubicTo(xTip, r.top() + h * 0.58,
                 xMid, r.top() + h * 0.57,
                 xMid, r.top() + h * 0.70);
    path.cubicTo(xMid, r.top() + h * 0.86,
                 xMid, r.bottom() - h * 0.025,
                 xOuter, r.bottom());
    return path;
}

QPainterPath starPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0)
        return QPainterPath();

    QPainterPath path;
    constexpr int points = 10;
    for (int i = 0; i < points; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
        const qreal factor = (i % 2 == 0) ? 1.0 : 0.42;
        const QPointF point(center.x() + qCos(angle) * rx * factor,
                            center.y() + qSin(angle) * ry * factor);
        if (i == 0)
            path.moveTo(point);
        else
            path.lineTo(point);
    }
    path.closeSubpath();
    return path;
}

QPainterPath calloutPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0)
        return QPainterPath();

    const QRectF body(r.left(), r.top(), w, h * 0.78);
    const qreal radius = qMin(w, h) * 0.16;
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(body, radius, radius);

    QPainterPath tail;
    tail.moveTo(r.left() + w * 0.20, body.bottom() - 0.5);
    tail.lineTo(r.left() + w * 0.12, r.bottom());
    tail.lineTo(r.left() + w * 0.38, body.bottom() - 0.5);
    tail.closeSubpath();

    return bodyPath.united(tail);
}

} // namespace

QList<ShapeType> allShapeTypes()
{
    return {
        ShapeType::Rectangle,
        ShapeType::RoundedRectangle,
        ShapeType::Ellipse,
        ShapeType::Triangle,
        ShapeType::Diamond,
        ShapeType::Star,
        ShapeType::Arrow,
        ShapeType::DoubleArrow,
        ShapeType::Line,
        ShapeType::BraceLeft,
        ShapeType::BraceRight,
        ShapeType::Callout
    };
}

QString shapeTypeName(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return QStringLiteral("矩形");
    case ShapeType::RoundedRectangle:
        return QStringLiteral("圆角矩形");
    case ShapeType::Ellipse:
        return QStringLiteral("圆形 / 椭圆");
    case ShapeType::Triangle:
        return QStringLiteral("三角形");
    case ShapeType::Diamond:
        return QStringLiteral("菱形");
    case ShapeType::Star:
        return QStringLiteral("五角星");
    case ShapeType::Arrow:
        return QStringLiteral("箭头");
    case ShapeType::DoubleArrow:
        return QStringLiteral("双向箭头");
    case ShapeType::Line:
        return QStringLiteral("直线");
    case ShapeType::BraceLeft:
        return QStringLiteral("左大括号");
    case ShapeType::BraceRight:
        return QStringLiteral("右大括号");
    case ShapeType::Callout:
        return QStringLiteral("对话气泡");
    }
    return QStringLiteral("矩形");
}

QString shapeTypeToString(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return QStringLiteral("rectangle");
    case ShapeType::RoundedRectangle:
        return QStringLiteral("rounded_rectangle");
    case ShapeType::Ellipse:
        return QStringLiteral("ellipse");
    case ShapeType::Triangle:
        return QStringLiteral("triangle");
    case ShapeType::Diamond:
        return QStringLiteral("diamond");
    case ShapeType::Star:
        return QStringLiteral("star");
    case ShapeType::Arrow:
        return QStringLiteral("arrow");
    case ShapeType::DoubleArrow:
        return QStringLiteral("double_arrow");
    case ShapeType::Line:
        return QStringLiteral("line");
    case ShapeType::BraceLeft:
        return QStringLiteral("brace_left");
    case ShapeType::BraceRight:
        return QStringLiteral("brace_right");
    case ShapeType::Callout:
        return QStringLiteral("callout");
    }
    return QStringLiteral("rectangle");
}

ShapeType shapeTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("rounded_rectangle")
        || normalized == QStringLiteral("roundedrect")
        || normalized == QStringLiteral("roundrect")) {
        return ShapeType::RoundedRectangle;
    }
    if (normalized == QStringLiteral("ellipse") || normalized == QStringLiteral("circle")
        || normalized == QStringLiteral("oval")) {
        return ShapeType::Ellipse;
    }
    if (normalized == QStringLiteral("triangle"))
        return ShapeType::Triangle;
    if (normalized == QStringLiteral("diamond") || normalized == QStringLiteral("rhombus"))
        return ShapeType::Diamond;
    if (normalized == QStringLiteral("star"))
        return ShapeType::Star;
    if (normalized == QStringLiteral("arrow"))
        return ShapeType::Arrow;
    if (normalized == QStringLiteral("double_arrow") || normalized == QStringLiteral("doublearrow")
        || normalized == QStringLiteral("bidirectional")) {
        return ShapeType::DoubleArrow;
    }
    if (normalized == QStringLiteral("line"))
        return ShapeType::Line;
    if (normalized == QStringLiteral("brace_left") || normalized == QStringLiteral("leftbrace")
        || normalized == QStringLiteral("braceleft")) {
        return ShapeType::BraceLeft;
    }
    if (normalized == QStringLiteral("brace_right") || normalized == QStringLiteral("rightbrace")
        || normalized == QStringLiteral("braceright")) {
        return ShapeType::BraceRight;
    }
    if (normalized == QStringLiteral("callout") || normalized == QStringLiteral("bubble")
        || normalized == QStringLiteral("speech")) {
        return ShapeType::Callout;
    }
    if (normalized == QStringLiteral("rect") || normalized == QStringLiteral("rectangle"))
        return ShapeType::Rectangle;
    return ShapeType::Rectangle;
}

QSizeF defaultShapeSize(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
        return QSizeF(300.0, 190.0);
    case ShapeType::Ellipse:
        return QSizeF(240.0, 180.0);
    case ShapeType::Triangle:
        return QSizeF(260.0, 210.0);
    case ShapeType::Diamond:
        return QSizeF(230.0, 180.0);
    case ShapeType::Star:
        return QSizeF(190.0, 190.0);
    case ShapeType::Arrow:
        return QSizeF(340.0, 120.0);
    case ShapeType::DoubleArrow:
        return QSizeF(380.0, 120.0);
    case ShapeType::Line:
        return QSizeF(320.0, 72.0);
    case ShapeType::BraceLeft:
    case ShapeType::BraceRight:
        return QSizeF(100.0, 300.0);
    case ShapeType::Callout:
        return QSizeF(300.0, 200.0);
    }
    return QSizeF(300.0, 190.0);
}

bool shapeTypeIsFilled(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
    case ShapeType::Ellipse:
    case ShapeType::Triangle:
    case ShapeType::Diamond:
    case ShapeType::Star:
    case ShapeType::Callout:
        return true;
    case ShapeType::Arrow:
    case ShapeType::DoubleArrow:
    case ShapeType::Line:
    case ShapeType::BraceLeft:
    case ShapeType::BraceRight:
        return false;
    }
    return false;
}

bool shapeTypeIsStrokeOnly(ShapeType type)
{
    return type == ShapeType::Line || type == ShapeType::BraceLeft
        || type == ShapeType::BraceRight;
}

QIcon makeShapeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale),
                   static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    QRectF rect(3.0, 4.0, 18.0, 16.0);
    if (type == ShapeType::BraceLeft || type == ShapeType::BraceRight)
        rect = QRectF(6.0, 2.0, 12.0, 20.0);
    else if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow
             || type == ShapeType::Line) {
        rect = QRectF(2.0, 7.0, 20.0, 10.0);
    }

    const QPainterPath path = ShapeItem::pathFor(type, rect);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    } else if (shapeTypeIsStrokeOnly(type)) {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
    } else {
        painter.setPen(pen);
        QColor fill = color;
        fill.setAlpha(50);
        painter.setBrush(fill);
    }
    painter.drawPath(path);
    painter.end();

    return QIcon(pixmap);
}

ShapeItem::ShapeItem(ShapeType type, QGraphicsItem* parent)
    : BoardItem(parent)
    , m_type(type)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
    setContentSize(defaultShapeSize(type));
}

QString ShapeItem::displayName() const
{
    return shapeTypeName(m_type);
}

void ShapeItem::setShapeType(ShapeType type)
{
    if (m_type == type)
        return;
    m_type = type;
    update();
}

void ShapeItem::setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth)
{
    m_strokeColor = strokeColor.isValid() ? strokeColor : QColor(30, 41, 59);
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_strokeWidth = qBound<qreal>(0.5, strokeWidth, 48.0);
    update();
}

ShapeItemState ShapeItem::state() const
{
    ShapeItemState result;
    result.type = m_type;
    result.strokeColor = m_strokeColor;
    result.fillColor = m_fillColor;
    result.strokeWidth = m_strokeWidth;
    result.pos = pos();
    result.size = contentSize();
    return result;
}

void ShapeItem::applyState(const ShapeItemState& state)
{
    m_type = state.type;
    setStyle(state.strokeColor, state.fillColor, state.strokeWidth);
    setPos(state.pos);
    setContentSize(state.size.isEmpty() ? defaultShapeSize(state.type) : state.size);
    update();
}

QPainterPath ShapeItem::shape() const
{
    const QPainterPath path = pathFor(m_type, contentRect());
    if (!shapeTypeIsStrokeOnly(m_type))
        return path;

    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(5.0, m_strokeWidth + 6.0));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path);
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0)
        return QPainterPath();

    QPainterPath path;
    switch (type) {
    case ShapeType::Rectangle:
        path.addRect(r);
        break;
    case ShapeType::RoundedRectangle: {
        const qreal radius = qMin(r.width(), r.height()) * 0.16;
        path.addRoundedRect(r, radius, radius);
        break;
    }
    case ShapeType::Ellipse:
        path.addEllipse(r);
        break;
    case ShapeType::Triangle:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
        break;
    case ShapeType::Diamond:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.center().x(), r.bottom());
        path.lineTo(r.left(), r.center().y());
        path.closeSubpath();
        break;
    case ShapeType::Star:
        path = starPath(r);
        break;
    case ShapeType::Arrow: {
        const qreal headLength = qMin(r.width() * 0.45,
                                      qMax(r.height() * 0.75, r.width() * 0.22));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal headBase = r.right() - headLength;
        path.moveTo(r.left(), cy - shaftHalf);
        path.lineTo(headBase, cy - shaftHalf);
        path.lineTo(headBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(headBase, r.bottom());
        path.lineTo(headBase, cy + shaftHalf);
        path.lineTo(r.left(), cy + shaftHalf);
        path.closeSubpath();
        break;
    }
    case ShapeType::DoubleArrow: {
        const qreal headLength = qMin(r.width() * 0.45,
                                      qMin(r.width() * 0.30,
                                           qMax(r.height() * 0.70, r.width() * 0.16)));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal leftBase = r.left() + headLength;
        const qreal rightBase = r.right() - headLength;
        path.moveTo(r.left(), cy);
        path.lineTo(leftBase, r.top());
        path.lineTo(leftBase, cy - shaftHalf);
        path.lineTo(rightBase, cy - shaftHalf);
        path.lineTo(rightBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(rightBase, r.bottom());
        path.lineTo(rightBase, cy + shaftHalf);
        path.lineTo(leftBase, cy + shaftHalf);
        path.lineTo(leftBase, r.bottom());
        path.closeSubpath();
        break;
    }
    case ShapeType::Line:
        path.moveTo(r.left(), r.center().y());
        path.lineTo(r.right(), r.center().y());
        break;
    case ShapeType::BraceLeft:
        path = bracePath(r);
        break;
    case ShapeType::BraceRight: {
        const QPainterPath left = bracePath(r);
        QTransform mirror;
        mirror.translate(r.left() + r.right(), 0.0);
        mirror.scale(-1.0, 1.0);
        path = mirror.map(left);
        break;
    }
    case ShapeType::Callout:
        path = calloutPath(r);
        break;
    }
    return path;
}

void ShapeItem::paintContent(QPainter* painter)
{
    const QRectF rect = contentRect();
    if (rect.isEmpty())
        return;

    const QPainterPath path = pathFor(m_type, rect);
    if (path.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_type == ShapeType::Arrow || m_type == ShapeType::DoubleArrow) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_strokeColor);
        painter->drawPath(path);
    } else if (shapeTypeIsStrokeOnly(m_type)) {
        QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine,
                 Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    } else {
        QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine,
                 Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(m_fillColor.alpha() > 0 ? QBrush(m_fillColor) : QBrush(Qt::NoBrush));
        painter->drawPath(path);
    }
    painter->restore();
}

QJsonObject ShapeItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("shapeType"), shapeTypeToString(m_type));
    json.insert(QStringLiteral("strokeColor"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillColor"), m_fillColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("strokeWidth"), m_strokeWidth);
    return json;
}

void ShapeItem::fromJson(const QJsonObject& json)
{
    m_type = shapeTypeFromString(json.value(QStringLiteral("shapeType")).toString());

    m_strokeColor = QColor(json.value(QStringLiteral("strokeColor"))
                               .toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid())
        m_strokeColor = QColor(30, 41, 59);

    if (json.contains(QStringLiteral("fillColor"))) {
        m_fillColor = QColor(json.value(QStringLiteral("fillColor")).toString());
        if (!m_fillColor.isValid())
            m_fillColor = QColor(Qt::transparent);
    } else {
        m_fillColor = m_strokeColor;
        m_fillColor.setAlpha(36);
    }

    m_strokeWidth = qBound<qreal>(
        0.5, json.value(QStringLiteral("strokeWidth")).toDouble(4.0), 48.0);

    applyBaseJson(json);
    if (contentSize().isEmpty())
        setContentSize(defaultShapeSize(m_type));
    update();
}
