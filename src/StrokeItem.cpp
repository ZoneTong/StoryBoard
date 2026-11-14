#include "StrokeItem.h"

#include "CanvasScene.h"
#include "StrokeGeometry.h"
#include "StrokePath.h"
#include "StrokeSmoothing.h"

#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QtMath>
#include <utility>

bool StrokeItem::GeometryState::operator==(const GeometryState& other) const
{
    if (dot != other.dot || !StrokeSmoothing::samePoints(points, other.points)) {
        return false;
    }
    return qAbs(pos.x() - other.pos.x()) < StrokeSmoothing::StateEpsilon &&
           qAbs(pos.y() - other.pos.y()) < StrokeSmoothing::StateEpsilon &&
           qAbs(contentSize.width() - other.contentSize.width()) < StrokeSmoothing::StateEpsilon &&
           qAbs(contentSize.height() - other.contentSize.height()) < StrokeSmoothing::StateEpsilon &&
           qAbs(baseSize.width() - other.baseSize.width()) < StrokeSmoothing::StateEpsilon &&
           qAbs(baseSize.height() - other.baseSize.height()) < StrokeSmoothing::StateEpsilon;
}

StrokeItem::StrokeItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    setZValue(0.0);
}

void StrokeItem::beginStroke(const QPointF& scenePos, const QColor& color, qreal width)
{
    m_color = color;
    m_penWidth = qMax<qreal>(0.5, width);
    m_pad = m_penWidth / 2.0 + 2.0;
    m_dot = false;
    m_finished = false;
    m_points.clear();
    m_points.append(QPointF(0.0, 0.0));
    setPos(scenePos);
    normalizeToContent();
}

void StrokeItem::setStyle(const QColor& color, qreal width)
{
    m_color = color.isValid() ? color : QColor(30, 41, 59);
    m_penWidth = qMax<qreal>(0.5, width);
    rebuildPath();
    update();
}

void StrokeItem::appendScenePoint(const QPointF& scenePos)
{
    if (m_finished || m_points.isEmpty()) {
        return;
    }

    const QPointF local = scenePos - pos();
    const qreal minDistance = 1.0 / qMax<qreal>(0.25, viewScale());
    if (QLineF(m_points.constLast(), local).length() < minDistance) {
        return;
    }

    m_dot = false;
    m_points.append(local);
    normalizeToContent();
}

void StrokeItem::setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width,
                                 qreal smoothingTolerance, bool linearPath)
{
    m_color = color.isValid() ? color : QColor(30, 41, 59);
    m_penWidth = qMax<qreal>(0.5, width);
    m_pad = m_penWidth / 2.0 + 2.0;
    m_linearPath = linearPath;
    m_points.clear();
    m_dot = false;
    m_finished = false;

    if (scenePoints.isEmpty()) {
        m_finished = true;
        return;
    }

    // 以首点为原点记局部坐标，normalizeToContent() 会再按内容边界对齐一次。
    const QPointF origin = scenePoints.constFirst();
    setPos(origin);
    m_points.reserve(scenePoints.size());
    for (const QPointF& point : scenePoints) {
        m_points.append(point - origin);
    }

    normalizeToContent();
    if (smoothingTolerance > 0.0) {
        smoothStroke(smoothingTolerance);
    }
    normalizeToContent();
    rebuildPath();
    m_finished = true;
    update();
}

void StrokeItem::endStroke(qreal smoothingTolerance)
{
    if (m_finished) {
        return;
    }

    if (m_points.isEmpty()) {
        m_finished = true;
        return;
    }

    if (m_points.size() < 2 || pathLength() < 2.5) {
        QRectF bounds(m_points.constFirst(), QSizeF(0.0, 0.0));
        for (const QPointF& point : std::as_const(m_points)) {
            bounds = bounds.united(QRectF(point, QSizeF(0.0, 0.0)));
        }
        m_dot = true;
        m_points = {bounds.center()};
    } else {
        m_dot = false;
        if (smoothingTolerance > 0.0) {
            smoothStroke(smoothingTolerance);
        }
    }

    normalizeToContent();
    rebuildPath();
    m_finished = true;
}

bool StrokeItem::smoothStroke(qreal tolerance)
{
    if (tolerance <= 0.0 || m_dot || m_points.size() < 3) {
        return false;
    }

    QVector<QPointF> smoothed = m_points;
    qreal chordLength = 0.0;
    if (StrokeSmoothing::looksLikeStraightLine(smoothed, tolerance, chordLength)) {
        // 用户想画直线：收成首尾两点，避免保留手抖造成的微小波浪。
        smoothed = {smoothed.constFirst(), smoothed.constLast()};
    } else {
        const qreal rdpTolerance = qMax<qreal>(1.0, tolerance * 0.75);
        smoothed = StrokeSmoothing::simplifyRdp(smoothed, rdpTolerance);
        if (smoothed.size() > 2) {
            const int passes = qBound(1, qRound(tolerance / 5.0), 3);
            smoothed = StrokeSmoothing::chaikinSmooth(smoothed, passes);
        }
    }

    if (StrokeSmoothing::samePoints(smoothed, m_points)) {
        return false;
    }

    m_points = smoothed;
    m_dot = false;
    normalizeToContent();
    rebuildPath();
    update();
    return true;
}

StrokeItem::GeometryState StrokeItem::geometryState() const
{
    return GeometryState{m_points, pos(), contentSize(), m_baseSize, m_dot};
}

void StrokeItem::applyGeometryState(const GeometryState& state)
{
    m_points = state.points;
    m_baseSize = state.baseSize;
    m_dot = state.dot;
    setPos(state.pos);
    setContentSize(state.contentSize);
    rebuildPath();
    update();
}

qreal StrokeItem::pathLength() const
{
    return StrokePath::length(m_points);
}

void StrokeItem::normalizeToContent()
{
    if (m_points.isEmpty()) {
        return;
    }

    QRectF bounds;
    if (m_points.size() == 1 || m_dot) {
        bounds = QRectF(m_points.constFirst(), QSizeF(0.0, 0.0));
    } else {
        qreal minX = m_points.constFirst().x();
        qreal maxX = minX;
        qreal minY = m_points.constFirst().y();
        qreal maxY = minY;
        for (const QPointF& point : std::as_const(m_points)) {
            minX = qMin(minX, point.x());
            maxX = qMax(maxX, point.x());
            minY = qMin(minY, point.y());
            maxY = qMax(maxY, point.y());
        }
        bounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }

    QPointF shift;
    if (m_dot || m_points.size() == 1) {
        shift = QPointF(m_pad - bounds.center().x(), m_pad - bounds.center().y());
    } else {
        shift = QPointF(m_pad - bounds.left(), m_pad - bounds.top());
    }

    if (!shift.isNull()) {
        for (QPointF& point : m_points) {
            point += shift;
        }
        setPos(pos() - shift);
    }

    if (m_dot || m_points.size() == 1) {
        m_baseSize = QSizeF(2.0 * m_pad, 2.0 * m_pad);
    } else {
        m_baseSize = QSizeF(bounds.width() + 2.0 * m_pad, bounds.height() + 2.0 * m_pad);
    }
    m_baseSize = QSizeF(qMax<qreal>(1.0, m_baseSize.width()), qMax<qreal>(1.0, m_baseSize.height()));

    rebuildPath();
    setContentSize(m_baseSize);
}

void StrokeItem::rebuildPath()
{
    m_path = StrokePath::build(m_points, m_dot, m_linearPath, m_penWidth);
}

void StrokeItem::paintContent(QPainter* painter)
{
    StrokeGeometry::render(painter, m_path, m_baseSize, contentSize(), m_color, m_penWidth);
}

QPainterPath StrokeItem::outlineShape() const
{
    return StrokeGeometry::outline(m_path, m_baseSize, contentSize());
}

QPainterPath StrokeItem::paintedShape() const
{
    // 完全透明的笔画画不出东西，也就不该算作遮挡。
    if (m_color.alpha() == 0) {
        return QPainterPath();
    }

    const QPainterPath centerLine = outlineShape();
    return StrokeGeometry::painted(centerLine, m_baseSize, contentSize(), m_penWidth);
}

QPainterPath StrokeItem::shape() const
{
    QPainterPath result = StrokeGeometry::hitScaled(m_path, m_baseSize, contentSize(), m_penWidth, viewScale());

    // Selection handles sit on the content corners; make them clickable even when
    // the stroke itself does not pass through those points.
    if (isSelected() && isEditable() && hasSingleSelection()) {
        for (int handle = TopLeft; handle <= Left; ++handle) {
            result.addRect(handleRect(static_cast<Handle>(handle)).adjusted(-2.0, -2.0, 2.0, 2.0));
        }
    }
    return result;
}

QJsonObject StrokeItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("color"), m_color.name(QColor::HexArgb));
    json.insert(QStringLiteral("width"), m_penWidth);
    json.insert(QStringLiteral("baseWidth"), m_baseSize.width());
    json.insert(QStringLiteral("baseHeight"), m_baseSize.height());
    json.insert(QStringLiteral("dot"), m_dot);
    if (m_linearPath) {
        json.insert(QStringLiteral("linear"), true);
    }

    QJsonArray points;
    for (const QPointF& point : m_points) {
        QJsonArray pair;
        pair.append(point.x());
        pair.append(point.y());
        points.append(pair);
    }
    json.insert(QStringLiteral("points"), points);
    return json;
}

void StrokeItem::fromJson(const QJsonObject& json)
{
    m_color = QColor(json.value(QStringLiteral("color")).toString(QStringLiteral("#ff1e293b")));
    if (!m_color.isValid()) {
        m_color = QColor(30, 41, 59);
    }
    m_penWidth = qMax<qreal>(0.5, json.value(QStringLiteral("width")).toDouble(4.0));
    m_pad = m_penWidth / 2.0 + 2.0;
    m_dot = json.value(QStringLiteral("dot")).toBool(false);
    m_linearPath = json.value(QStringLiteral("linear")).toBool(false);

    m_points.clear();
    const QJsonArray points = json.value(QStringLiteral("points")).toArray();
    m_points.reserve(points.size());
    for (const QJsonValue& value : points) {
        const QJsonArray pair = value.toArray();
        if (pair.size() >= 2) {
            m_points.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
        }
    }

    const QSizeF storedBase(json.value(QStringLiteral("baseWidth")).toDouble(1.0),
                            json.value(QStringLiteral("baseHeight")).toDouble(1.0));
    m_baseSize = QSizeF(qMax<qreal>(1.0, storedBase.width()), qMax<qreal>(1.0, storedBase.height()));

    applyBaseJson(json);
    m_finished = true;
    rebuildPath();
}
