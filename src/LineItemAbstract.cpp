#include "LineItemAbstract.h"

#include "CanvasItemVisitor.h"
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

void LineItemAbstract::Accept(CanvasItemVisitor& visitor)
{
    visitor.VisitLine(*this);
}

bool LineItemAbstract::GeometryState::operator==(const GeometryState& other) const
{
    if (hasDot != other.hasDot || segments.size() != other.segments.size() ||
        !StrokeSmoothing::SamePoint(dotCenter, other.dotCenter)) {
        return false;
    }
    for (int i = 0; i < segments.size(); ++i) {
        if (segments.at(i) != other.segments.at(i)) {
            return false;
        }
    }
    return qAbs(pos.x() - other.pos.x()) < StrokeSmoothing::STATE_EPSILON &&
           qAbs(pos.y() - other.pos.y()) < StrokeSmoothing::STATE_EPSILON &&
           qAbs(contentSize.width() - other.contentSize.width()) < StrokeSmoothing::STATE_EPSILON &&
           qAbs(contentSize.height() - other.contentSize.height()) < StrokeSmoothing::STATE_EPSILON &&
           qAbs(baseSize.width() - other.baseSize.width()) < StrokeSmoothing::STATE_EPSILON &&
           qAbs(baseSize.height() - other.baseSize.height()) < StrokeSmoothing::STATE_EPSILON;
}

ItemPropertySet LineItemAbstract::Properties() const
{
    // 描线三件套对五个形态统一生效；箭头 / 角度 / 波形参数由各自的子型追加。
    return CanvasItem::Properties().United(
        ItemPropertySet{ItemProperty::STROKE_STYLE, ItemProperty::STROKE_COLOR, ItemProperty::STROKE_WIDTH});
}

QVariant LineItemAbstract::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::STROKE_STYLE:
        return static_cast<int>(StrokeStyle());
    case ItemProperty::STROKE_COLOR:
        return m_strokeColor;
    case ItemProperty::STROKE_WIDTH:
        return m_strokeWidth;
    default:
        return CanvasItem::PropertyValue(property);
    }
}

void LineItemAbstract::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::STROKE_STYLE:
        SetStrokePattern(static_cast<Qt::PenStyle>(value.toInt()));
        break;
    case ItemProperty::STROKE_COLOR:
        SetStrokeColor(value.value<QColor>());
        break;
    case ItemProperty::STROKE_WIDTH:
        SetStrokeWidth(value.toDouble());
        break;
    default:
        CanvasItem::SetPropertyValue(property, value);
        break;
    }
}

QColor LineItemAbstract::StrokeColorOrDefault(const QColor& color) const
{
    return color.isValid() ? color : QColor(30, 41, 59);
}

void LineItemAbstract::SetStrokeColor(const QColor& color)
{
    StrokeAttributes::SetStrokeColor(color);
}

void LineItemAbstract::SetStrokeWidth(qreal width)
{
    const qreal resolved = qBound<qreal>(0.5, width, 48.0);
    if (qFuzzyCompare(m_strokeWidth + 1.0, resolved + 1.0)) {
        return;
    }
    m_strokeWidth = resolved;
    m_strokePadding = m_strokeWidth / 2.0 + 2.0;
    RebuildPath();
    prepareGeometryChange();
    update();
}

void LineItemAbstract::SetStrokePattern(Qt::PenStyle style)
{
    StrokeAttributes::SetStrokePattern(style);
}

void LineItemAbstract::SetArrowStyles(ArrowStyle start, ArrowStyle end)
{
    if (m_startArrow == start && m_endArrow == end) {
        return;
    }
    m_startArrow = start;
    m_endArrow = end;
    update();
}

void LineItemAbstract::SetStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width,
                                       qreal tolerance, bool polyline)
{
    m_strokeColor = StrokeColorOrDefault(color);
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

    m_segments = CurveGeometry::FitSegments(local, tolerance, polyline);
    NormalizeGeometry();
    RebuildPath();
    update();
}

QVector<QPointF> LineItemAbstract::StrokePoints() const
{
    return CurveGeometry::Knots(m_segments);
}

QRectF LineItemAbstract::SegmentBounds() const
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

void LineItemAbstract::NormalizeGeometry()
{
    const QRectF bounds = SegmentBounds();
    if (bounds.isNull()) {
        return;
    }

    const bool singlePoint = IsSinglePointGeometry();
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

    RebuildPath();
    SetContentSize(m_baseSize);
}

void LineItemAbstract::RebuildPath()
{
    m_path = CurveGeometry::BuildPath(m_segments);
}

bool LineItemAbstract::SimplifyCurve(qreal tolerance)
{
    Q_UNUSED(tolerance)
    return false;
}

void LineItemAbstract::PaintContent(QPainter* painter)
{
    StrokeGeometry::Render(painter, m_path, m_baseSize, ContentSize(), m_strokeColor, m_strokeWidth, m_strokeStyle);

    // 两端箭头：端点取贝塞尔段的首尾节点，与描线共用同一套基坐标 → 内容尺寸缩放，
    // 箭头随描线一起缩放；单点笔迹没有方向，不画箭头。
    if (!ArrowsAvailable() || (!ArrowStyleIsHead(m_startArrow) && !ArrowStyleIsHead(m_endArrow)) ||
        m_segments.isEmpty() || m_baseSize.isEmpty() || m_baseSize.width() <= 0.0 || m_baseSize.height() <= 0.0) {
        return;
    }
    const QVector<QPointF> knots = CurveGeometry::Knots(m_segments);
    if (knots.size() < 2) {
        return;
    }
    const qreal scaleX = ContentSize().width() / m_baseSize.width();
    const qreal scaleY = ContentSize().height() / m_baseSize.height();
    painter->save();
    painter->scale(scaleX, scaleY);
    PaintArrowHead(painter, knots.constFirst(), knots.at(1), m_strokeWidth, m_strokeColor, m_startArrow);
    PaintArrowHead(painter, knots.constLast(), knots.at(knots.size() - 2), m_strokeWidth, m_strokeColor, m_endArrow);
    painter->restore();
}

QPainterPath LineItemAbstract::OutlineShape() const
{
    return StrokeGeometry::Outline(m_path, m_baseSize, ContentSize());
}

QPainterPath LineItemAbstract::PaintedShape() const
{
    // 完全透明的描线画不出东西，也就不该算作遮挡。
    if (m_strokeColor.alpha() == 0) {
        return QPainterPath();
    }
    const QPainterPath centerLine = OutlineShape();
    return StrokeGeometry::Painted(centerLine, m_baseSize, ContentSize(), m_strokeWidth);
}

QPainterPath LineItemAbstract::shape() const
{
    QPainterPath result = StrokeGeometry::HitScaled(m_path, m_baseSize, ContentSize(), m_strokeWidth, ViewScale());

    // Selection handles sit on the content corners; make them clickable even when
    // the stroke itself does not pass through those points.
    if (isSelected() && IsEditable() && HasSingleSelection()) {
        for (int handle = TOP_LEFT; handle <= LEFT; ++handle) {
            result.addRect(HandleRect(static_cast<Handle>(handle)).adjusted(-2.0, -2.0, 2.0, 2.0));
        }
    }
    return result;
}

LineItemAbstract::GeometryState LineItemAbstract::geometryState() const
{
    return GeometryState{m_segments, pos(), ContentSize(), m_baseSize, false, QPointF()};
}

void LineItemAbstract::ApplyGeometryState(const GeometryState& state)
{
    m_segments = state.segments;
    m_baseSize = state.baseSize;
    setPos(state.pos);
    SetContentSize(state.contentSize);
    RebuildPath();
    update();
}

QJsonObject LineItemAbstract::ToJson() const
{
    QJsonObject json = BaseJson();
    json.insert(QStringLiteral("color"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("width"), m_strokeWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_strokeStyle));
    json.insert(QStringLiteral("baseWidth"), m_baseSize.width());
    json.insert(QStringLiteral("baseHeight"), m_baseSize.height());
    json.insert(QStringLiteral("startArrow"), ArrowStyleToString(m_startArrow));
    json.insert(QStringLiteral("endArrow"), ArrowStyleToString(m_endArrow));
    if (m_polyline) {
        json.insert(QStringLiteral("polyline"), true);
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

void LineItemAbstract::FromJson(const QJsonObject& json)
{
    m_strokeColor = QColor(json.value(QStringLiteral("color")).toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid()) {
        m_strokeColor = QColor(30, 41, 59);
    }
    m_strokeWidth = qMax<qreal>(0.5, json.value(QStringLiteral("width")).toDouble(4.0));
    m_strokeStyle = static_cast<Qt::PenStyle>(json.value(QStringLiteral("strokeStyle")).toInt(Qt::SolidLine));
    m_strokePadding = m_strokeWidth / 2.0 + 2.0;
    m_startArrow = ArrowStyleFromString(json.value(QStringLiteral("startArrow")).toString());
    m_endArrow = ArrowStyleFromString(json.value(QStringLiteral("endArrow")).toString());
    m_polyline = json.value(QStringLiteral("polyline")).toBool(false);

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

    ApplyBaseJson(json);
    RebuildPath();
}
