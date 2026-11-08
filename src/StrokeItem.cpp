#include "StrokeItem.h"

#include "BoardScene.h"

#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>

#include <utility>

StrokeItem::StrokeItem(QGraphicsItem* parent)
    : BoardItem(parent)
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
    if (m_finished || m_points.isEmpty())
        return;

    const QPointF local = scenePos - pos();
    const qreal minDistance = 1.0 / qMax<qreal>(0.25, viewScale());
    if (QLineF(m_points.constLast(), local).length() < minDistance)
        return;

    m_dot = false;
    m_points.append(local);
    normalizeToContent();
}

void StrokeItem::endStroke()
{
    if (m_finished)
        return;

    if (m_points.isEmpty()) {
        m_finished = true;
        return;
    }

    if (m_points.size() < 2 || pathLength() < 2.5) {
        QRectF bounds(m_points.constFirst(), QSizeF(0.0, 0.0));
        for (const QPointF& point : std::as_const(m_points))
            bounds = bounds.united(QRectF(point, QSizeF(0.0, 0.0)));
        m_dot = true;
        m_points = { bounds.center() };
    } else {
        m_dot = false;
    }

    normalizeToContent();
    rebuildPath();
    m_finished = true;
}

qreal StrokeItem::pathLength() const
{
    qreal length = 0.0;
    for (int i = 1; i < m_points.size(); ++i)
        length += QLineF(m_points.at(i - 1), m_points.at(i)).length();
    return length;
}

void StrokeItem::normalizeToContent()
{
    if (m_points.isEmpty())
        return;

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
    if (m_dot || m_points.size() == 1)
        shift = QPointF(m_pad - bounds.center().x(), m_pad - bounds.center().y());
    else
        shift = QPointF(m_pad - bounds.left(), m_pad - bounds.top());

    if (!shift.isNull()) {
        for (QPointF& point : m_points)
            point += shift;
        setPos(pos() - shift);
    }

    if (m_dot || m_points.size() == 1) {
        m_baseSize = QSizeF(2.0 * m_pad, 2.0 * m_pad);
    } else {
        m_baseSize = QSizeF(bounds.width() + 2.0 * m_pad, bounds.height() + 2.0 * m_pad);
    }
    m_baseSize = QSizeF(qMax<qreal>(1.0, m_baseSize.width()),
                        qMax<qreal>(1.0, m_baseSize.height()));

    rebuildPath();
    setContentSize(m_baseSize);
}

void StrokeItem::rebuildPath()
{
    m_path = QPainterPath();
    if (m_points.isEmpty())
        return;

    if (m_dot || m_points.size() == 1) {
        const QPointF center = m_points.constFirst();
        const qreal radius = m_penWidth / 2.0;
        m_path.addEllipse(center, radius, radius);
        return;
    }

    QPainterPath path;
    path.moveTo(m_points.constFirst());
    if (m_points.size() == 2) {
        path.lineTo(m_points.constLast());
    } else {
        for (int i = 1; i < m_points.size() - 1; ++i) {
            const QPointF middle = (m_points.at(i) + m_points.at(i + 1)) / 2.0;
            path.quadTo(m_points.at(i), middle);
        }
        path.lineTo(m_points.constLast());
    }
    m_path = path;
}

void StrokeItem::paintContent(QPainter* painter)
{
    if (m_baseSize.isEmpty() || m_path.isEmpty())
        return;

    const qreal scaleX = contentSize().width() / m_baseSize.width();
    const qreal scaleY = contentSize().height() / m_baseSize.height();

    painter->save();
    painter->scale(scaleX, scaleY);
    QPen pen(m_color);
    pen.setWidthF(m_penWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);
    painter->restore();
}

QPainterPath StrokeItem::outlineShape() const
{
    if (m_baseSize.isEmpty() || m_path.isEmpty())
        return QPainterPath();

    QTransform transform;
    transform.scale(contentSize().width() / m_baseSize.width(),
                    contentSize().height() / m_baseSize.height());
    return transform.map(m_path);
}

QPainterPath StrokeItem::paintedShape() const
{
    // 完全透明的笔画画不出东西，也就不该算作遮挡。
    if (m_color.alpha() == 0)
        return QPainterPath();

    const QPainterPath centerLine = outlineShape();
    if (centerLine.isEmpty())
        return centerLine;

    const qreal scaleX = contentSize().width() / m_baseSize.width();
    const qreal scaleY = contentSize().height() / m_baseSize.height();
    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(1.0, m_penWidth * (scaleX + scaleY) / 2.0));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(centerLine);
}

QPainterPath StrokeItem::shape() const
{
    if (m_baseSize.isEmpty())
        return QPainterPath();

    const qreal scaleX = contentSize().width() / m_baseSize.width();
    const qreal scaleY = contentSize().height() / m_baseSize.height();
    QTransform transform;
    transform.scale(scaleX, scaleY);
    const QPainterPath scaled = transform.map(m_path);

    QPainterPathStroker stroker;
    const qreal averageScale = (scaleX + scaleY) / 2.0;
    const qreal tolerance = qMin<qreal>(4.0 / viewScale(), 16.0);
    stroker.setWidth(qMax<qreal>(2.0, m_penWidth * averageScale) + tolerance);
    QPainterPath result = stroker.createStroke(scaled);
    result.addPath(scaled);

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
    if (!m_color.isValid())
        m_color = QColor(30, 41, 59);
    m_penWidth = qMax<qreal>(0.5, json.value(QStringLiteral("width")).toDouble(4.0));
    m_pad = m_penWidth / 2.0 + 2.0;
    m_dot = json.value(QStringLiteral("dot")).toBool(false);

    m_points.clear();
    const QJsonArray points = json.value(QStringLiteral("points")).toArray();
    m_points.reserve(points.size());
    for (const QJsonValue& value : points) {
        const QJsonArray pair = value.toArray();
        if (pair.size() >= 2)
            m_points.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
    }

    const QSizeF storedBase(json.value(QStringLiteral("baseWidth")).toDouble(1.0),
                            json.value(QStringLiteral("baseHeight")).toDouble(1.0));
    m_baseSize = QSizeF(qMax<qreal>(1.0, storedBase.width()), qMax<qreal>(1.0, storedBase.height()));

    applyBaseJson(json);
    m_finished = true;
    rebuildPath();
}
