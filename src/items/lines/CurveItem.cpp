#include "items/lines/CurveItem.h"

#include "items/CanvasItemVisitor.h"
#include "items/geometry/CurveGeometry.h"
#include "items/geometry/StrokePath.h"
#include "items/geometry/StrokeSmoothing.h"

#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QtMath>

CurveItem::CurveItem(QGraphicsItem* parent) : LineAbstractItem(parent) {}

ItemPropertySet CurveItem::Properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineAbstractItem::Properties().United(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant CurveItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    default:
        return LineAbstractItem::PropertyValue(property);
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
        LineAbstractItem::SetPropertyValue(property, value);
        break;
    }
}

QString CurveItem::DisplayName() const
{
    return UiText(QStringLiteral("Curve"));
}

void CurveItem::BeginStroke(const QPointF& scenePos, const QColor& color, qreal width)
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

void CurveItem::AppendScenePoint(const QPointF& scenePos)
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
    LineAbstractItem::SetStrokePoints(scenePoints, color, width, tolerance, polyline);
    m_finished = true;
}

void CurveItem::EndStroke(qreal tolerance)
{
    if (m_finished) {
        return;
    }

    if (m_rawPoints.isEmpty()) {
        m_finished = true;
        return;
    }

    if (m_rawPoints.size() < 2 || StrokePath::Length(m_rawPoints) < 2.5) {
        QRectF bounds(m_rawPoints.constFirst(), QSizeF(0.0, 0.0));
        for (const QPointF& point : std::as_const(m_rawPoints)) {
            bounds = bounds.united(QRectF(point, QSizeF(0.0, 0.0)));
        }
        m_hasDot = true;
        m_segments.clear();
        m_rawPoints = {bounds.center()};
    } else {
        m_hasDot = false;
        m_segments = CurveGeometry::FitSegments(m_rawPoints, tolerance, m_polyline);
        m_rawPoints.clear();
    }

    m_drawing = false;
    m_finished = true;
    NormalizeGeometry();
    RebuildPath();
}

bool CurveItem::IsEmpty() const
{
    return !m_hasDot && m_segments.isEmpty() && m_rawPoints.isEmpty();
}

void CurveItem::Accept(CanvasItemVisitor& visitor)
{
    // 曲线是描线族的叶子：只上报曲线分支，宿主不必再按类强转判断“能不能简化”。
    visitor.VisitCurve(*this);
}

bool CurveItem::IsSinglePointGeometry() const
{
    return m_hasDot || (m_drawing && m_rawPoints.size() == 1);
}

bool CurveItem::CanSimplify() const
{
    // 单点笔迹没有可简化内容；不足三个节点也拍不出新形状。
    return !m_hasDot && !m_drawing && StrokePoints().size() >= 3;
}

bool CurveItem::SimplifyCurve(qreal tolerance)
{
    if (m_hasDot || m_segments.isEmpty() || tolerance <= 0.0) {
        return false;
    }

    // §D10：容差是“用户看到的像素”，不是基准坐标里的单位。先按当前内容尺寸把路径缩放到
    // 显示空间再做 RDP，否则缩到 25% 时 2 单位容差只有 0.5 像素，几乎等于没简化。
    const QSizeF base = m_baseSize;
    const QSizeF content = ContentSize();
    const qreal scaleX = base.width() > 0.0 ? content.width() / base.width() : 1.0;
    const qreal scaleY = base.height() > 0.0 ? content.height() / base.height() : 1.0;
    const QTransform toDisplay = QTransform::fromScale(scaleX, scaleY);

    QVector<QPointF> points = CurveGeometry::Flatten(m_path, toDisplay);
    if (points.size() < 3) {
        return false;
    }
    points = StrokeSmoothing::SimplifyRdp(points, qMax<qreal>(0.5, tolerance));
    if (points.size() < 2) {
        return false;
    }

    // 折线回到基准坐标后再拟合，控制点与 m_baseSize 始终同一空间。
    const qreal invScaleX = qAbs(scaleX) > 1e-9 ? 1.0 / scaleX : 1.0;
    const qreal invScaleY = qAbs(scaleY) > 1e-9 ? 1.0 / scaleY : 1.0;
    for (QPointF& point : points) {
        point = QPointF(point.x() * invScaleX, point.y() * invScaleY);
    }

    QVector<CurveSegment> simplified = CurveGeometry::FitSegments(points, 0.0, m_polyline);
    if (simplified == m_segments) {
        return false;
    }

    const QPointF oldPos = pos();
    m_segments = simplified;
    NormalizeGeometry();
    RebuildPath();

    // NormalizeGeometry 会把内容尺寸重置成新的 baseSize（即 1:1），把用户调过的尺寸和
    // 位置补回去，否则“简化一下曲线就跳回原始大小”。归一化把笔迹挪了 shift、把 pos 减了
    // shift，在缩放 s 下视觉偏移是 shift*(1-s)，按轴向补回来。
    const QPointF shift = oldPos - pos();
    SetContentSize(QSizeF(m_baseSize.width() * scaleX, m_baseSize.height() * scaleY));
    setPos(pos() + QPointF(shift.x() * (1.0 - scaleX), shift.y() * (1.0 - scaleY)));
    update();
    return true;
}

CurveItem::GeometryState CurveItem::geometryState() const
{
    GeometryState state;
    state.segments = m_segments;
    state.pos = pos();
    state.contentSize = ContentSize();
    state.baseSize = m_baseSize;
    state.hasDot = m_hasDot;
    state.dotCenter = m_rawPoints.isEmpty() ? QPointF() : m_rawPoints.constFirst();
    return state;
}

void CurveItem::ApplyGeometryState(const GeometryState& state)
{
    LineAbstractItem::ApplyGeometryState(state);
    m_hasDot = state.hasDot;
    m_rawPoints = m_hasDot ? QVector<QPointF>{state.dotCenter} : QVector<QPointF>();
    m_drawing = false;
    m_finished = true;
    RebuildPath();
    update();
}

bool CurveItem::HasSelectionFrame() const
{
    // 单点笔迹没有可拖端点，保留外接框作为选中反馈；其余曲线只用节点方块。
    return IsSinglePointGeometry();
}

HandleSpecList CurveItem::CustomHandles() const
{
    HandleSpecList specs;
    const QVector<QPointF> knots = StrokePoints();
    if (knots.size() < 2) {
        return specs;
    }
    // 首尾是绑定端点；中间只在转角明显的拐点处给方块，避免平滑采样点铺满整条曲线，
    // 拖中段时随手命中节点手柄而把曲线拉长。
    HandleSpec start;
    start.id = CanvasItem::CUSTOM_0;
    start.position = knots.constFirst();
    start.behavior = HandleBehavior::ARC_ENDPOINT;
    start.cursor = Qt::SizeAllCursor;
    start.sizeScale = 0.9;
    specs.append(start);
    for (int i = 1; i < knots.size() - 1; ++i) {
        if (CurveGeometry::TurnDegrees(knots.at(i - 1), knots.at(i), knots.at(i + 1)) < 30.0) {
            continue;
        }
        HandleSpec knot;
        knot.id = CanvasItem::CUSTOM_1 + i;
        knot.position = knots.at(i);
        knot.behavior = HandleBehavior::STATIC;
        knot.cursor = Qt::SizeAllCursor;
        knot.sizeScale = 0.8;
        specs.append(knot);
    }
    HandleSpec end;
    end.id = CanvasItem::CUSTOM_1;
    end.position = knots.constLast();
    end.behavior = HandleBehavior::ARC_ENDPOINT;
    end.cursor = Qt::SizeAllCursor;
    end.sizeScale = 0.9;
    specs.append(end);
    return specs;
}

bool CurveItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    if (handle == CanvasItem::CUSTOM_0 || handle == CanvasItem::CUSTOM_1) {
        return true;
    }
    // CUSTOM_1 之后依次是中间拐点：槽位 11 对应第 1 个拐点。
    return handle > CanvasItem::CUSTOM_1 && handle - CanvasItem::CUSTOM_1 < StrokePoints().size() - 1;
}

bool CurveItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle == CanvasItem::CUSTOM_0 || handle == CanvasItem::CUSTOM_1) {
        // 与直线同一份吸附 / 解绑口径，避免两处分别维护绑定语义。
        return DragEndpointWithBinding(handle == CanvasItem::CUSTOM_0, scenePos);
    }
    return MoveKnot(handle - CanvasItem::CUSTOM_1, scenePos);
}

bool CurveItem::MoveKnot(int index, const QPointF& scenePoint)
{
    const QVector<QPointF> knots = StrokePoints();
    if (index <= 0 || index >= knots.size() - 1 || index - 1 >= m_segments.size() || index >= m_segments.size()) {
        return false;
    }
    const QPointF delta = mapFromScene(scenePoint) - knots.at(index);
    // 接缝两侧各有一组控制柄：一起平移，曲率形状不变，只把拐点搬到新位置。
    m_segments[index - 1].p1 += delta;
    m_segments[index - 1].c2 += delta;
    m_segments[index].p0 += delta;
    m_segments[index].c1 += delta;
    NormalizeGeometry();
    update();
    return true;
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
        m_path = CurveGeometry::BuildDotPath(m_rawPoints.constFirst(), m_strokeWidth / 2.0);
        return;
    }
    if (m_drawing) {
        m_path = StrokePath::Build(m_rawPoints, false, m_polyline, m_strokeWidth);
        return;
    }
    m_path = CurveGeometry::BuildPath(m_segments);
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
    QJsonObject json = LineAbstractItem::ToJson();
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

    LineAbstractItem::FromJson(json);

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
