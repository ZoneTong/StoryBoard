#include "LineItemAbstract.h"

#include "StrokeGeometry.h"
#include "StrokeSmoothing.h"

#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QtMath>

LineItemAbstract::LineItemAbstract(QGraphicsItem* parent) : CanvasItem(parent)
{
    setZValue(0.0);
}

bool LineItemAbstract::GeometryState::operator==(const GeometryState& other) const
{
    if (hasDot != other.hasDot || segments.size() != other.segments.size() ||
        !StrokeSmoothing::samePoint(dotCenter, other.dotCenter)) {
        return false;
    }
    for (int i = 0; i < segments.size(); ++i) {
        if (segments.at(i) != other.segments.at(i)) {
            return false;
        }
    }
    return qAbs(pos.x() - other.pos.x()) < StrokeSmoothing::StateEpsilon &&
           qAbs(pos.y() - other.pos.y()) < StrokeSmoothing::StateEpsilon &&
           qAbs(contentSize.width() - other.contentSize.width()) < StrokeSmoothing::StateEpsilon &&
           qAbs(contentSize.height() - other.contentSize.height()) < StrokeSmoothing::StateEpsilon &&
           qAbs(baseSize.width() - other.baseSize.width()) < StrokeSmoothing::StateEpsilon &&
           qAbs(baseSize.height() - other.baseSize.height()) < StrokeSmoothing::StateEpsilon;
}

ItemPropertySet LineItemAbstract::properties() const
{
    // 描线三件套对五个形态统一生效；箭头 / 角度 / 波形参数由各自的子型追加。
    return CanvasItem::properties().united(
        ItemPropertySet{ItemProperty::STROKE_STYLE, ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH});
}

QVariant LineItemAbstract::propertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::STROKE_STYLE:
        return static_cast<int>(m_strokePattern);
    case ItemProperty::STROKE_COLOR:
        return m_strokeColor;
    case ItemProperty::STROKE_WIDTH:
        return m_strokeWidth;
    default:
        return CanvasItem::propertyValue(property);
    }
}

void LineItemAbstract::setPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::STROKE_STYLE:
        setStrokePattern(static_cast<Qt::PenStyle>(value.toInt()));
        break;
    case ItemProperty::STROKE_COLOR:
        setStrokeColor(value.value<QColor>());
        break;
    case ItemProperty::STROKE_WIDTH:
        setStrokeWidth(value.toDouble());
        break;
    default:
        CanvasItem::setPropertyValue(property, value);
        break;
    }
}

QColor LineItemAbstract::strokeColorOrDefault(const QColor& color) const
{
    return color.isValid() ? color : QColor(30, 41, 59);
}

void LineItemAbstract::setStrokeColor(const QColor& color)
{
    const QColor resolved = strokeColorOrDefault(color);
    if (m_strokeColor == resolved) {
        return;
    }
    m_strokeColor = resolved;
    update();
}

void LineItemAbstract::setStrokeWidth(qreal width)
{
    const qreal resolved = qMax<qreal>(0.5, width);
    if (qFuzzyCompare(m_strokeWidth, resolved)) {
        return;
    }
    m_strokeWidth = resolved;
    m_strokePadding = m_strokeWidth / 2.0 + 2.0;
    rebuildPath();
    update();
}

void LineItemAbstract::setStrokePattern(Qt::PenStyle style)
{
    if (m_strokePattern == style) {
        return;
    }
    m_strokePattern = style;
    update();
}

void LineItemAbstract::setArrowStyles(ArrowStyle start, ArrowStyle end)
{
    if (m_startArrow == start && m_endArrow == end) {
        return;
    }
    m_startArrow = start;
    m_endArrow = end;
    update();
}

void LineItemAbstract::setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width,
                                       qreal tolerance, bool polyline)
{
    m_strokeColor = strokeColorOrDefault(color);
    m_strokeWidth = qMax<qreal>(0.5, width);
    m_strokePadding = m_strokeWidth / 2.0 + 2.0;
    m_polyline = polyline;
    m_segments.clear();

    if (scenePoints.isEmpty()) {
        m_path = QPainterPath();
        update();
        return;
    }

    const QPointF origin = scenePoints.constFirst();
    setPos(origin);
    QVector<QPointF> local;
    local.reserve(scenePoints.size());
    for (const QPointF& point : scenePoints) {
        local.append(point - origin);
    }

    m_segments = CurveGeometry::fitSegments(local, tolerance, polyline);
    normalizeGeometry();
    rebuildPath();
    update();
}

QVector<QPointF> LineItemAbstract::strokePoints() const
{
    return CurveGeometry::knots(m_segments);
}

QRectF LineItemAbstract::segmentBounds() const
{
    qreal minX = 0.0;
    qreal minY = 0.0;
    qreal maxX = 0.0;
    qreal maxY = 0.0;
    bool hasBounds = false;
    auto include = [&](const QPointF& point) {
        if (!hasBounds) {
            minX = maxX = point.x();
            minY = maxY = point.y();
            hasBounds = true;
            return;
        }
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    };
    for (const CurveSegment& segment : std::as_const(m_segments)) {
        include(segment.p0);
        include(segment.c1);
        include(segment.c2);
        include(segment.p1);
    }
    if (!hasBounds) {
        return QRectF();
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void LineItemAbstract::normalizeGeometry()
{
    const QRectF bounds = segmentBounds();
    if (bounds.isNull()) {
        return;
    }

    const bool singlePoint = isSinglePointGeometry();
    QPointF shift;
    if (singlePoint) {
        shift = QPointF(m_strokePadding - bounds.center().x(), m_strokePadding - bounds.center().y());
        m_baseSize = QSizeF(2.0 * m_strokePadding, 2.0 * m_strokePadding);
    } else {
        shift = QPointF(m_strokePadding - bounds.left(), m_strokePadding - bounds.top());
        m_baseSize = QSizeF(bounds.width() + 2.0 * m_strokePadding, bounds.height() + 2.0 * m_strokePadding);
    }
    m_baseSize = QSizeF(qMax<qreal>(1.0, m_baseSize.width()), qMax<qreal>(1.0, m_baseSize.height()));

    if (!shift.isNull()) {
        for (CurveSegment& segment : m_segments) {
            segment.p0 += shift;
            segment.c1 += shift;
            segment.c2 += shift;
            segment.p1 += shift;
        }
        setPos(pos() - shift);
    }

    rebuildPath();
    setContentSize(m_baseSize);
}

void LineItemAbstract::rebuildPath()
{
    m_path = CurveGeometry::buildPath(m_segments);
}

bool LineItemAbstract::simplifyCurve(qreal tolerance)
{
    Q_UNUSED(tolerance)
    return false;
}

void LineItemAbstract::paintContent(QPainter* painter)
{
    StrokeGeometry::render(painter, m_path, m_baseSize, contentSize(), m_strokeColor, m_strokeWidth, m_strokePattern);

    // 两端箭头：端点取贝塞尔段的首尾节点，与描线共用同一套基坐标 → 内容尺寸缩放，
    // 箭头随描线一起缩放；单点笔迹没有方向，不画箭头。
    if (!arrowsAvailable() || (!arrowStyleIsHead(m_startArrow) && !arrowStyleIsHead(m_endArrow)) ||
        m_segments.isEmpty() || m_baseSize.isEmpty() || m_baseSize.width() <= 0.0 || m_baseSize.height() <= 0.0) {
        return;
    }
    const QVector<QPointF> knots = CurveGeometry::knots(m_segments);
    if (knots.size() < 2) {
        return;
    }
    const qreal scaleX = contentSize().width() / m_baseSize.width();
    const qreal scaleY = contentSize().height() / m_baseSize.height();
    painter->save();
    painter->scale(scaleX, scaleY);
    paintArrowHead(painter, knots.constFirst(), knots.at(1), m_strokeWidth, m_strokeColor, m_startArrow);
    paintArrowHead(painter, knots.constLast(), knots.at(knots.size() - 2), m_strokeWidth, m_strokeColor, m_endArrow);
    painter->restore();
}

QPainterPath LineItemAbstract::outlineShape() const
{
    return StrokeGeometry::outline(m_path, m_baseSize, contentSize());
}

QPainterPath LineItemAbstract::paintedShape() const
{
    // 完全透明的描线画不出东西，也就不该算作遮挡。
    if (m_strokeColor.alpha() == 0) {
        return QPainterPath();
    }
    const QPainterPath centerLine = outlineShape();
    return StrokeGeometry::painted(centerLine, m_baseSize, contentSize(), m_strokeWidth);
}

QPainterPath LineItemAbstract::shape() const
{
    QPainterPath result = StrokeGeometry::hitScaled(m_path, m_baseSize, contentSize(), m_strokeWidth, viewScale());

    // Selection handles sit on the content corners; make them clickable even when
    // the stroke itself does not pass through those points.
    if (isSelected() && isEditable() && hasSingleSelection()) {
        for (int handle = TOP_LEFT; handle <= LEFT; ++handle) {
            result.addRect(handleRect(static_cast<Handle>(handle)).adjusted(-2.0, -2.0, 2.0, 2.0));
        }
    }
    return result;
}

LineItemAbstract::GeometryState LineItemAbstract::geometryState() const
{
    return GeometryState{m_segments, pos(), contentSize(), m_baseSize, false, QPointF()};
}

void LineItemAbstract::applyGeometryState(const GeometryState& state)
{
    m_segments = state.segments;
    m_baseSize = state.baseSize;
    setPos(state.pos);
    setContentSize(state.contentSize);
    rebuildPath();
    update();
}

QJsonObject LineItemAbstract::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("color"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("width"), m_strokeWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokePattern));
    json.insert(QStringLiteral("baseWidth"), m_baseSize.width());
    json.insert(QStringLiteral("baseHeight"), m_baseSize.height());
    json.insert(QStringLiteral("startArrow"), arrowStyleToString(m_startArrow));
    json.insert(QStringLiteral("endArrow"), arrowStyleToString(m_endArrow));
    if (m_polyline) {
        json.insert(QStringLiteral("linear"), true);
    }

    QJsonArray segments;
    for (const CurveSegment& segment : std::as_const(m_segments)) {
        QJsonArray values;
        values.append(segment.p0.x());
        values.append(segment.p0.y());
        values.append(segment.c1.x());
        values.append(segment.c1.y());
        values.append(segment.c2.x());
        values.append(segment.c2.y());
        values.append(segment.p1.x());
        values.append(segment.p1.y());
        segments.append(values);
    }
    json.insert(QStringLiteral("segments"), segments);
    return json;
}

void LineItemAbstract::fromJson(const QJsonObject& json)
{
    m_strokeColor = QColor(json.value(QStringLiteral("color")).toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid()) {
        m_strokeColor = QColor(30, 41, 59);
    }
    m_strokeWidth = qMax<qreal>(0.5, json.value(QStringLiteral("width")).toDouble(4.0));
    m_strokePattern = static_cast<Qt::PenStyle>(json.value(QStringLiteral("strokeStyle")).toInt(Qt::SolidLine));
    m_strokePadding = m_strokeWidth / 2.0 + 2.0;
    m_startArrow = arrowStyleFromString(json.value(QStringLiteral("startArrow")).toString());
    m_endArrow = arrowStyleFromString(json.value(QStringLiteral("endArrow")).toString());
    m_polyline = json.value(QStringLiteral("linear")).toBool(false);

    m_segments.clear();
    const QJsonArray segments = json.value(QStringLiteral("segments")).toArray();
    m_segments.reserve(segments.size());
    for (const QJsonValue& value : segments) {
        const QJsonArray values = value.toArray();
        if (values.size() >= 8) {
            m_segments.append(CurveSegment{QPointF(values.at(0).toDouble(), values.at(1).toDouble()),
                                           QPointF(values.at(2).toDouble(), values.at(3).toDouble()),
                                           QPointF(values.at(4).toDouble(), values.at(5).toDouble()),
                                           QPointF(values.at(6).toDouble(), values.at(7).toDouble())});
        }
    }

    const QSizeF storedBase(json.value(QStringLiteral("baseWidth")).toDouble(1.0),
                            json.value(QStringLiteral("baseHeight")).toDouble(1.0));
    m_baseSize = QSizeF(qMax<qreal>(1.0, storedBase.width()), qMax<qreal>(1.0, storedBase.height()));

    applyBaseJson(json);
    rebuildPath();
}
