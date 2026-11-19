#include "CurveItem.h"

#include "CanvasScene.h"
#include "StrokeGeometry.h"
#include "StrokePath.h"
#include "StrokeSmoothing.h"

#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QtMath>

namespace {

QString curveKindToString(CurveKind kind)
{
    switch (kind) {
    case CurveKind::Line:
        return QStringLiteral("line");
    case CurveKind::Arc:
        return QStringLiteral("arc");
    case CurveKind::Sine:
        return QStringLiteral("sine");
    case CurveKind::Brace:
        return QStringLiteral("brace");
    case CurveKind::Freehand:
        break;
    }
    return QStringLiteral("freehand");
}

CurveKind curveKindFromString(const QString& name)
{
    if (name == QStringLiteral("line")) {
        return CurveKind::Line;
    }
    if (name == QStringLiteral("arc")) {
        return CurveKind::Arc;
    }
    if (name == QStringLiteral("sine")) {
        return CurveKind::Sine;
    }
    if (name == QStringLiteral("brace")) {
        return CurveKind::Brace;
    }
    return CurveKind::Freehand;
}

} // namespace

bool CurveItem::GeometryState::operator==(const GeometryState& other) const
{
    if (dot != other.dot || segments.size() != other.segments.size() ||
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

CurveItem::CurveItem(QGraphicsItem* parent) : CanvasItem(parent)
{
    setZValue(0.0);
}

QString CurveItem::displayName() const
{
    return uiText(QStringLiteral("Curve"));
}

void CurveItem::beginStroke(const QPointF& scenePos, const QColor& color, qreal width)
{
    m_color = color;
    m_penWidth = qMax<qreal>(0.5, width);
    m_pad = m_penWidth / 2.0 + 2.0;
    m_dot = false;
    m_drawing = true;
    m_finished = false;
    m_segments.clear();
    m_rawPoints.clear();
    m_rawPoints.append(QPointF(0.0, 0.0));
    setPos(scenePos);
    normalizeToContent();
}

void CurveItem::setStyle(const QColor& color, qreal width)
{
    m_color = color.isValid() ? color : QColor(30, 41, 59);
    m_penWidth = qMax<qreal>(0.5, width);
    rebuildPath();
    update();
}

void CurveItem::setStrokeStyle(Qt::PenStyle style)
{
    if (m_penStyle == style) {
        return;
    }
    m_penStyle = style;
    update();
}

void CurveItem::appendScenePoint(const QPointF& scenePos)
{
    if (m_finished || !m_drawing || m_rawPoints.isEmpty()) {
        return;
    }

    const QPointF local = scenePos - pos();
    const qreal minDistance = 1.0 / qMax<qreal>(0.25, viewScale());
    if (QLineF(m_rawPoints.constLast(), local).length() < minDistance) {
        return;
    }

    m_dot = false;
    m_rawPoints.append(local);
    normalizeToContent();
}

void CurveItem::setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width, qreal tolerance,
                                bool linearPath)
{
    m_color = color.isValid() ? color : QColor(30, 41, 59);
    m_penWidth = qMax<qreal>(0.5, width);
    m_pad = m_penWidth / 2.0 + 2.0;
    m_linearPath = linearPath;
    m_rawPoints.clear();
    m_segments.clear();
    m_dot = false;
    m_drawing = false;
    m_finished = false;

    if (scenePoints.isEmpty()) {
        m_finished = true;
        return;
    }

    const QPointF origin = scenePoints.constFirst();
    setPos(origin);
    QVector<QPointF> local;
    local.reserve(scenePoints.size());
    for (const QPointF& point : scenePoints) {
        local.append(point - origin);
    }

    m_segments = CurveGeometry::fitSegments(local, tolerance, linearPath);
    normalizeToContent();
    rebuildPath();
    m_finished = true;
    update();
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
        m_dot = true;
        m_segments.clear();
        m_rawPoints = {bounds.center()};
    } else {
        m_dot = false;
        m_segments = CurveGeometry::fitSegments(m_rawPoints, tolerance, m_linearPath);
        m_rawPoints.clear();
    }

    m_drawing = false;
    m_finished = true;
    normalizeToContent();
    rebuildPath();
}

bool CurveItem::isEmpty() const
{
    return !m_dot && m_segments.isEmpty() && m_rawPoints.isEmpty();
}

bool CurveItem::simplifyCurve(qreal tolerance)
{
    // 直线 / 弧线 / 正弦 / 花括号都是程序化几何，简化会改掉它们的定义，直接跳过。
    // 手绘笔迹与图片线稿提取（source = LineArt）属于 Freehand 子型，放开简化。
    if (m_curveKind != CurveKind::Freehand) {
        return false;
    }
    if (m_dot || m_segments.isEmpty() || tolerance <= 0.0) {
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

    QVector<CurveSegment> simplified = CurveGeometry::fitSegments(points, 0.0, m_linearPath);
    if (simplified == m_segments) {
        return false;
    }

    m_segments = simplified;
    normalizeToContent();
    rebuildPath();
    update();
    return true;
}

CurveItem::GeometryState CurveItem::geometryState() const
{
    return GeometryState{m_segments, pos(), contentSize(),
                         m_baseSize, m_dot, m_rawPoints.isEmpty() ? QPointF() : m_rawPoints.constFirst()};
}

void CurveItem::applyGeometryState(const GeometryState& state)
{
    m_segments = state.segments;
    m_baseSize = state.baseSize;
    m_dot = state.dot;
    m_rawPoints = m_dot ? QVector<QPointF>{state.dotCenter} : QVector<QPointF>();
    m_drawing = false;
    m_finished = true;
    setPos(state.pos);
    setContentSize(state.contentSize);
    rebuildPath();
    update();
}

QRectF CurveItem::geometryBounds() const
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

    if (m_drawing || m_dot) {
        for (const QPointF& point : std::as_const(m_rawPoints)) {
            include(point);
        }
    } else {
        for (const CurveSegment& segment : std::as_const(m_segments)) {
            include(segment.p0);
            include(segment.c1);
            include(segment.c2);
            include(segment.p1);
        }
    }
    if (!hasBounds) {
        return QRectF();
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void CurveItem::normalizeToContent()
{
    const QRectF bounds = geometryBounds();
    const bool singlePoint = m_dot || (m_drawing && m_rawPoints.size() == 1);
    if (bounds.isNull() && !singlePoint) {
        return;
    }

    QPointF shift;
    if (singlePoint) {
        shift = QPointF(m_pad - bounds.center().x(), m_pad - bounds.center().y());
        m_dot = true;
        m_baseSize = QSizeF(2.0 * m_pad, 2.0 * m_pad);
    } else {
        shift = QPointF(m_pad - bounds.left(), m_pad - bounds.top());
        m_baseSize = QSizeF(bounds.width() + 2.0 * m_pad, bounds.height() + 2.0 * m_pad);
    }
    m_baseSize = QSizeF(qMax<qreal>(1.0, m_baseSize.width()), qMax<qreal>(1.0, m_baseSize.height()));

    if (!shift.isNull()) {
        if (m_drawing || m_dot) {
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

    rebuildPath();
    setContentSize(m_baseSize);
}

void CurveItem::rebuildPath()
{
    if (m_dot && !m_rawPoints.isEmpty()) {
        m_path = CurveGeometry::buildDotPath(m_rawPoints.constFirst(), m_penWidth / 2.0);
        return;
    }
    if (m_drawing) {
        m_path = StrokePath::build(m_rawPoints, false, m_linearPath, m_penWidth);
        return;
    }
    m_path = CurveGeometry::buildPath(m_segments);
}

void CurveItem::paintContent(QPainter* painter)
{
    StrokeGeometry::render(painter, m_path, m_baseSize, contentSize(), m_color, m_penWidth, m_penStyle);

    // 需求 3.12：曲线两端也可带箭头。端点取贝塞尔段的首尾节点，
    // 与描线共用同一套基坐标 → 内容尺寸缩放，箭头随曲线一起缩放。
    if ((arrowStyleIsHead(m_startArrow) || arrowStyleIsHead(m_endArrow)) && !m_dot && m_segments.size() >= 1 &&
        !m_baseSize.isEmpty()) {
        const QVector<QPointF> knots = CurveGeometry::knots(m_segments);
        if (knots.size() >= 2) {
            const qreal scaleX = contentSize().width() / m_baseSize.width();
            const qreal scaleY = contentSize().height() / m_baseSize.height();
            painter->save();
            painter->scale(scaleX, scaleY);
            paintArrowHead(painter, knots.constFirst(), knots.at(1), m_penWidth, m_color, m_startArrow);
            paintArrowHead(painter, knots.constLast(), knots.at(knots.size() - 2), m_penWidth, m_color, m_endArrow);
            painter->restore();
        }
    }
}

void CurveItem::setSource(CurveSource source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    update();
}

void CurveItem::setCurveKind(CurveKind kind)
{
    if (m_curveKind == kind) {
        return;
    }
    m_curveKind = kind;
    update();
}

void CurveItem::setArrowStyles(ArrowStyle start, ArrowStyle end)
{
    if (m_startArrow == start && m_endArrow == end) {
        return;
    }
    m_startArrow = start;
    m_endArrow = end;
    update();
}

QPainterPath CurveItem::outlineShape() const
{
    return StrokeGeometry::outline(m_path, m_baseSize, contentSize());
}

QPainterPath CurveItem::paintedShape() const
{
    // 完全透明的曲线画不出东西，也就不该算作遮挡。
    if (m_color.alpha() == 0) {
        return QPainterPath();
    }

    const QPainterPath centerLine = outlineShape();
    return StrokeGeometry::painted(centerLine, m_baseSize, contentSize(), m_penWidth);
}

QPainterPath CurveItem::shape() const
{
    QPainterPath result = StrokeGeometry::hitScaled(m_path, m_baseSize, contentSize(), m_penWidth, viewScale());

    // Selection handles sit on the content corners; make them clickable even when
    // the curve itself does not pass through those points.
    if (isSelected() && isEditable() && hasSingleSelection()) {
        for (int handle = TopLeft; handle <= Left; ++handle) {
            result.addRect(handleRect(static_cast<Handle>(handle)).adjusted(-2.0, -2.0, 2.0, 2.0));
        }
    }
    return result;
}

QJsonObject CurveItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("color"), m_color.name(QColor::HexArgb));
    json.insert(QStringLiteral("width"), m_penWidth);
    json.insert(QStringLiteral("strokeStyle"), static_cast<int>(m_penStyle));
    json.insert(QStringLiteral("baseWidth"), m_baseSize.width());
    json.insert(QStringLiteral("baseHeight"), m_baseSize.height());
    json.insert(QStringLiteral("dot"), m_dot);
    json.insert(QStringLiteral("startArrow"), arrowStyleToString(m_startArrow));
    json.insert(QStringLiteral("endArrow"), arrowStyleToString(m_endArrow));
    if (m_linearPath) {
        json.insert(QStringLiteral("linear"), true);
    }
    if (m_source != CurveSource::HandDrawn) {
        json.insert(QStringLiteral("source"), QStringLiteral("line_art"));
    }
    json.insert(QStringLiteral("curveKind"), curveKindToString(m_curveKind));

    if (m_dot && !m_rawPoints.isEmpty()) {
        QJsonArray center;
        center.append(m_rawPoints.constFirst().x());
        center.append(m_rawPoints.constFirst().y());
        json.insert(QStringLiteral("dotCenter"), center);
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

void CurveItem::fromJson(const QJsonObject& json)
{
    m_color = QColor(json.value(QStringLiteral("color")).toString(QStringLiteral("#ff1e293b")));
    if (!m_color.isValid()) {
        m_color = QColor(30, 41, 59);
    }
    m_penWidth = qMax<qreal>(0.5, json.value(QStringLiteral("width")).toDouble(4.0));
    m_penStyle = static_cast<Qt::PenStyle>(json.value(QStringLiteral("strokeStyle")).toInt(Qt::SolidLine));
    m_pad = m_penWidth / 2.0 + 2.0;
    m_dot = json.value(QStringLiteral("dot")).toBool(false);
    m_startArrow = arrowStyleFromString(json.value(QStringLiteral("startArrow")).toString());
    m_endArrow = arrowStyleFromString(json.value(QStringLiteral("endArrow")).toString());
    m_linearPath = json.value(QStringLiteral("linear")).toBool(false);
    m_curveKind = curveKindFromString(json.value(QStringLiteral("curveKind")).toString());
    // 来源只剩“手绘 / 图片线稿提取”两种，花括号等程序化曲线不再借 source 兼职。
    m_source =
        json.value(QStringLiteral("source")).toString() == QStringLiteral("line_art") ? CurveSource::LineArt
                                                                                      : CurveSource::HandDrawn;
    m_segments.clear();
    m_rawPoints.clear();
    m_drawing = false;

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

    if (m_dot) {
        const QJsonArray center = json.value(QStringLiteral("dotCenter")).toArray();
        const QPointF point(center.size() >= 2 ? center.at(0).toDouble() : m_pad,
                            center.size() >= 2 ? center.at(1).toDouble() : m_pad);
        m_rawPoints = {point};
    }

    const QSizeF storedBase(json.value(QStringLiteral("baseWidth")).toDouble(1.0),
                            json.value(QStringLiteral("baseHeight")).toDouble(1.0));
    m_baseSize = QSizeF(qMax<qreal>(1.0, storedBase.width()), qMax<qreal>(1.0, storedBase.height()));

    applyBaseJson(json);
    m_finished = true;
    rebuildPath();
}
