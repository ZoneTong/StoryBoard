#include "CurveItem.h"

#include "CurveGeometry.h"
#include "StrokePath.h"
#include "StrokeSmoothing.h"

#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QtMath>

CurveItem::CurveItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

ItemPropertySet CurveItem::Properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineItemAbstract::Properties().united(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant CurveItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    default:
        return LineItemAbstract::PropertyValue(property);
    }
}

void CurveItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::ARROW_START:
        SetArrowStyles(static_cast<ArrowStyle>(value.toInt()), EndArrow());
        break;
    case ItemProperty::ARROW_END:
        SetArrowStyles(StartArrow(), static_cast<ArrowStyle>(value.toInt()));
        break;
    default:
        LineItemAbstract::SetPropertyValue(property, value);
        break;
    }
}

QString CurveItem::DisplayName() const
{
    return uiText(QStringLiteral("Curve"));
}

void CurveItem::beginStroke(const QPointF& scenePos, const QColor& color, qreal width)
{
    m_strokeColor = StrokeColorOrDefault(color);
    m_strokeWidth = qMax<qreal>(0.5, width);
    m_strokePadding = m_strokeWidth / 2.0 + 2.0;
    m_hasDot = false;
    m_drawing = true;
    m_finished = false;
    m_segments.clear();
    m_rawPoints.clear();
    m_rawPoints.append(QPointF(0.0, 0.0));
    setPos(scenePos);
    NormalizeGeometry();
}

void CurveItem::appendScenePoint(const QPointF& scenePos)
{
    if (m_finished || !m_drawing || m_rawPoints.isEmpty()) {
        return;
    }

    const QPointF local = scenePos - pos();
    const qreal minDistance = 1.0 / qMax<qreal>(0.25, ViewScale());
    if (QLineF(m_rawPoints.constLast(), local).length() < minDistance) {
        return;
    }

    m_hasDot = false;
    m_rawPoints.append(local);
    CurveItem::NormalizeGeometry();
}

void CurveItem::SetStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width, qreal tolerance,
                                bool polyline)
{
    m_rawPoints.clear();
    m_hasDot = false;
    m_drawing = false;
    m_finished = false;
    LineItemAbstract::SetStrokePoints(scenePoints, color, width, tolerance, polyline);
    m_finished = true;
}

void CurveItem::endStroke(qreal tolerance)
{
    if (m_finished) {
        return;
    }

    if (m_rawPoints.isEmpty()) {
        m_finished = true;
        return;
    }

    if (m_rawPoints.size() < 2 || StrokePath::length(m_rawPoints) < 2.5) {
        QRectF bounds(m_rawPoints.constFirst(), QSizeF(0.0, 0.0));
        for (const QPointF& point : std::as_const(m_rawPoints)) {
            bounds = bounds.united(QRectF(point, QSizeF(0.0, 0.0)));
        }
        m_hasDot = true;
        m_segments.clear();
        m_rawPoints = {bounds.center()};
    } else {
        m_hasDot = false;
        m_segments = CurveGeometry::fitSegments(m_rawPoints, tolerance, m_polyline);
        m_rawPoints.clear();
    }

    m_drawing = false;
    m_finished = true;
    NormalizeGeometry();
    RebuildPath();
}

bool CurveItem::isEmpty() const
{
    return !m_hasDot && m_segments.isEmpty() && m_rawPoints.isEmpty();
}

bool CurveItem::IsSinglePointGeometry() const
{
    return m_hasDot || (m_drawing && m_rawPoints.size() == 1);
}

bool CurveItem::SimplifyCurve(qreal tolerance)
{
    if (m_hasDot || m_segments.isEmpty() || tolerance <= 0.0) {
        return false;
    }

    QVector<QPointF> points = CurveGeometry::flatten(m_path);
    if (points.size() < 3) {
        return false;
    }
    points = StrokeSmoothing::simplifyRdp(points, qMax<qreal>(0.5, tolerance));
    if (points.size() < 2) {
        return false;
    }

    QVector<CurveSegment> simplified = CurveGeometry::fitSegments(points, 0.0, m_polyline);
    if (simplified == m_segments) {
        return false;
    }

    m_segments = simplified;
    NormalizeGeometry();
    RebuildPath();
    update();
    return true;
}

CurveItem::GeometryState CurveItem::geometryState() const
{
    GeometryState state;
    state.segments = m_segments;
    state.pos = pos();
    state.contentSize = contentSize();
    state.baseSize = m_baseSize;
    state.hasDot = m_hasDot;
    state.dotCenter = m_rawPoints.isEmpty() ? QPointF() : m_rawPoints.constFirst();
    return state;
}

void CurveItem::ApplyGeometryState(const GeometryState& state)
{
    LineItemAbstract::ApplyGeometryState(state);
    m_hasDot = state.hasDot;
    m_rawPoints = m_hasDot ? QVector<QPointF>{state.dotCenter} : QVector<QPointF>();
    m_drawing = false;
    m_finished = true;
    RebuildPath();
    update();
}

void CurveItem::NormalizeGeometry()
{
    // 绘制期直接用采样点，收笔后才按拟合段计算；两套几何互斥，避免回读空集合。
    QVector<QPointF> points;
    if (m_drawing || m_hasDot) {
        points = m_rawPoints;
    } else {
        for (const CurveSegment& segment : std::as_const(m_segments)) {
            points.append(segment.p0);
            points.append(segment.c1);
            points.append(segment.c2);
            points.append(segment.p1);
        }
    }
    if (points.isEmpty()) {
        return;
    }

    qreal minX = points.constFirst().x();
    qreal maxX = minX;
    qreal minY = points.constFirst().y();
    qreal maxY = minY;
    for (const QPointF& point : points) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    const QRectF bounds(minX, minY, maxX - minX, maxY - minY);

    const bool singlePoint = IsSinglePointGeometry();
    QPointF shift;
    if (singlePoint) {
        shift = QPointF(m_strokePadding - bounds.center().x(), m_strokePadding - bounds.center().y());
        m_hasDot = true;
        m_baseSize = QSizeF(2.0 * m_strokePadding, 2.0 * m_strokePadding);
    } else {
        shift = QPointF(m_strokePadding - bounds.left(), m_strokePadding - bounds.top());
        m_baseSize = QSizeF(bounds.width() + 2.0 * m_strokePadding, bounds.height() + 2.0 * m_strokePadding);
    }
    m_baseSize = QSizeF(qMax<qreal>(1.0, m_baseSize.width()), qMax<qreal>(1.0, m_baseSize.height()));

    if (!shift.isNull()) {
        if (m_drawing || m_hasDot) {
            for (QPointF& point : m_rawPoints) {
                point += shift;
            }
        }
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

void CurveItem::RebuildPath()
{
    if (m_hasDot && !m_rawPoints.isEmpty()) {
        m_path = CurveGeometry::buildDotPath(m_rawPoints.constFirst(), m_strokeWidth / 2.0);
        return;
    }
    if (m_drawing) {
        m_path = StrokePath::build(m_rawPoints, false, m_polyline, m_strokeWidth);
        return;
    }
    m_path = CurveGeometry::buildPath(m_segments);
}

void CurveItem::SetStrokeSource(CurveSource source)
{
    if (m_strokeSource == source) {
        return;
    }
    m_strokeSource = source;
    update();
}

QJsonObject CurveItem::ToJson() const
{
    QJsonObject json = LineItemAbstract::ToJson();
    json.insert(QStringLiteral("dot"), m_hasDot);
    if (m_strokeSource != CurveSource::HAND_DRAWN) {
        json.insert(QStringLiteral("source"), QStringLiteral("line_art"));
    }
    if (m_hasDot && !m_rawPoints.isEmpty()) {
        QJsonArray center;
        center.append(m_rawPoints.constFirst().x());
        center.append(m_rawPoints.constFirst().y());
        json.insert(QStringLiteral("dotCenter"), center);
    }
    return json;
}

void CurveItem::FromJson(const QJsonObject& json)
{
    m_hasDot = json.value(QStringLiteral("dot")).toBool(false);
    // 来源只剩“手绘 / 图片线稿提取”两种。
    m_strokeSource = json.value(QStringLiteral("source")).toString() == QStringLiteral("line_art")
                         ? CurveSource::LINE_ART
                         : CurveSource::HAND_DRAWN;
    m_rawPoints.clear();

    LineItemAbstract::FromJson(json);

    if (m_hasDot) {
        const QJsonArray center = json.value(QStringLiteral("dotCenter")).toArray();
        const QPointF point(center.size() >= 2 ? center.at(0).toDouble() : m_strokePadding,
                            center.size() >= 2 ? center.at(1).toDouble() : m_strokePadding);
        m_rawPoints = {point};
    }

    m_drawing = false;
    m_finished = true;
    RebuildPath();
}
