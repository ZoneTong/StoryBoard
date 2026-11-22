#include "items/shapes/TriangleItem.h"

#include "items/CanvasItem.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/geometry/TriangleGeometry.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QtMath>
#include <cmath>

TriangleItem::TriangleItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF TriangleItem::DefaultContentSize() const
{
    return QSizeF(128.0, 111.0);
}

ItemPropertySet TriangleItem::Properties() const
{
    ItemPropertySet result = ShapeAbstractItem::Properties();
    result.Insert(ItemProperty::BOTTOM_ANGLE1);
    result.Insert(ItemProperty::BOTTOM_ANGLE2);
    return result;
}

QPainterPath TriangleItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::TrianglePath(rect, m_bottomAngle1, m_bottomAngle2);
}

HandleSpecList TriangleItem::CustomHandles() const
{
    HandleSpecList specs;
    if (IsCustomShape()) {
        return specs;
    }
    // 顶点 C 的手柄向下偏移，让开底角 90° 时与角顶点重合的角落手柄；
    // 形很矮时收在内框里。位置随视图缩放换算，保证屏幕上尺寸稳定。
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    const qreal inset = qMin<qreal>(18.0 / scale, qMax<qreal>(0.0, ContentRect().height() - 7.0 / scale));
    HandleSpec apex;
    apex.id = CanvasItem::CUSTOM_0;
    apex.position = triangleVertex(TriangleVertex::C) + QPointF(0.0, inset);
    apex.behavior = HandleBehavior::ANGLE;
    apex.cursor = Qt::SizeAllCursor;
    apex.replacesTopHandle = true;
    specs.append(apex);
    return specs;
}

void TriangleItem::SetTriangleAngles(qreal angle1, qreal angle2)
{
    TriangleGeometry::ClampAngles(&angle1, &angle2);
    if (qFuzzyCompare(m_bottomAngle1 + 2.0, angle1 + 2.0) && qFuzzyCompare(m_bottomAngle2 + 2.0, angle2 + 2.0)) {
        return;
    }
    // 角度优先：底边长度保持不变，包围盒宽高按新角度重新派生。
    const QSizeF newSize =
        TriangleGeometry::SizeForAngles(ContentSize(), m_bottomAngle1, m_bottomAngle2, angle1, angle2);
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
    m_syncingTriangle = true;
    SetContentSize(newSize);
    m_syncingTriangle = false;
    update();
}

void TriangleItem::SetTriangleAnglesFixedHeight(qreal angle1, qreal angle2, qreal height)
{
    TriangleGeometry::ClampAngles(&angle1, &angle2);
    // §3.1 拖顶点：AB 边上的垂直高度是输入，底边与包围框宽度随角度派生。
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
    m_syncingTriangle = true;
    SetContentSize(TriangleGeometry::SizeForHeight(qMax<qreal>(1.0, height), angle1, angle2));
    m_syncingTriangle = false;
    update();
}

void TriangleItem::ApplyState(const ShapeItemState& state)
{
    m_stateOldAngle1 = m_bottomAngle1;
    m_stateOldAngle2 = m_bottomAngle2;
    qreal angle1 = state.propertyExtras.value(ItemProperty::BOTTOM_ANGLE1, m_bottomAngle1).toDouble();
    qreal angle2 = state.propertyExtras.value(ItemProperty::BOTTOM_ANGLE2, m_bottomAngle2).toDouble();
    if (!IsCustomShape()) {
        TriangleGeometry::ClampAngles(&angle1, &angle2);
    }
    m_bottomAngle1 = angle1;
    m_bottomAngle2 = angle2;
    ShapeAbstractItem::ApplyState(state);
}

void TriangleItem::ApplyStateContentSize(const QSizeF& requested)
{
    const bool sameSize = qFuzzyCompare(requested.width() + 0.01, ContentSize().width() + 0.01) &&
                          qFuzzyCompare(requested.height() + 0.01, ContentSize().height() + 0.01);
    const bool angleChanged = !qFuzzyCompare(m_stateOldAngle1 + 2.0, m_bottomAngle1 + 2.0) ||
                              !qFuzzyCompare(m_stateOldAngle2 + 2.0, m_bottomAngle2 + 2.0);
    // 尺寸没变、只改角度：底边长度保持不变；其余情况按宽度与角度重派生高度。
    const bool preserveBase = !IsCustomShape() && sameSize && angleChanged;
    m_syncingTriangle = true;
    if (preserveBase) {
        SetContentSize(TriangleGeometry::SizeForAngles(ContentSize(), m_stateOldAngle1, m_stateOldAngle2,
                                                       m_bottomAngle1, m_bottomAngle2));
    } else {
        QSizeF target = requested;
        if (!IsCustomShape()) {
            target.setHeight(target.width() / TriangleGeometry::AspectRatio(m_bottomAngle1, m_bottomAngle2));
        }
        SetContentSize(target);
    }
    m_syncingTriangle = false;
}

bool TriangleItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    // 顶点 C 的内角手柄只属于三角形；自定义轮廓没有角度语义，不提供专用拖拽。
    if (handle != CanvasItem::CUSTOM_0 || IsCustomShape()) {
        return false;
    }
    m_apexDragBaseA = mapToScene(triangleVertex(TriangleVertex::A));
    m_apexDragBaseB = mapToScene(triangleVertex(TriangleVertex::B));
    m_apexDragHeight = qMax<qreal>(1.0, ContentSize().height());
    SetAngleReadoutVisible(true);
    return true;
}

bool TriangleItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 || IsCustomShape()) {
        return false;
    }
    // §3.1：拖顶点只改内角 A。AB 边上的垂直高度保持拖拽开始时的值，B 由底边长度
    // 与高度反算（cotB = base/h − cotA）；A 的钝角分支跟指针方向一致（D12），按 1° 取整。
    const QPointF base = m_apexDragBaseB - m_apexDragBaseA;
    const qreal baseLength = qMax<qreal>(1.0, std::hypot(base.x(), base.y()));
    const QPointF along = base / baseLength;
    const qreal fromA = QPointF::dotProduct(scenePos - m_apexDragBaseA, along);
    const qreal angleA = qRound(qRadiansToDegrees(qAtan2(m_apexDragHeight, fromA)));
    const qreal cotA = 1.0 / qTan(qDegreesToRadians(angleA));
    const qreal cotB = baseLength / m_apexDragHeight - cotA;
    const qreal angleB = qRadiansToDegrees(qAtan2(1.0, cotB));
    SetTriangleAnglesFixedHeight(angleA, angleB, m_apexDragHeight);
    SetAngleReadoutVisible(true);
    // 底边长度与高度都没变，把局部 A 点搬回拖拽开始时的场景位置即可，
    // B 会随同一条底边一起保持固定。
    const QPointF newA = mapToScene(triangleVertex(TriangleVertex::A));
    setPos(pos() + (m_apexDragBaseA - newA));
    return true;
}

void TriangleItem::FrameResizeDragged()
{
    // §3.1：三角形四角 / 边自由缩放时，A、B 读数随拖动显示（B 在 ContentResized 里反算）。
    if (!IsCustomShape()) {
        SetAngleReadoutVisible(true);
    }
}

void TriangleItem::ContentResized()
{
    if (IsCustomShape() || m_syncingTriangle) {
        return;
    }
    // §3.1：四角自由缩放后 A 保持不变，B 按新框宽高反算（cotB = w/h − cotA）；
    // 反算分支随新框宽高自然取值，绘制按新角度贴合包围盒。
    const QSizeF size = ContentSize();
    const qreal cotA = 1.0 / qTan(qDegreesToRadians(m_bottomAngle1));
    const qreal cotB = size.width() / qMax<qreal>(1.0, size.height()) - cotA;
    const qreal angleB = qRadiansToDegrees(qAtan2(1.0, cotB));
    m_bottomAngle2 = qBound<qreal>(5.0, angleB, 175.0 - m_bottomAngle1);
    update();
}

void TriangleItem::FromJson(const QJsonObject& json)
{
    // 载入期间先屏蔽三角形尺寸联动，避免 baseJson 恢复尺寸时反推高度、改动旧文件外观。
    m_syncingTriangle = true;
    ShapeAbstractItem::FromJson(json);
    m_syncingTriangle = false;
}

void TriangleItem::WriteJsonExtra(QJsonObject& json) const
{
    // 三角形以角度为真相，两个底角随画布保存。
    json.insert(QStringLiteral("bottomAngle1"), m_bottomAngle1);
    json.insert(QStringLiteral("bottomAngle2"), m_bottomAngle2);
    ShapeAbstractItem::WriteJsonExtra(json);
}

void TriangleItem::ReadJsonExtra(const QJsonObject& json)
{
    ShapeAbstractItem::ReadJsonExtra(json);
    if (IsCustomShape()) {
        return;
    }
    if (json.contains(QStringLiteral("bottomAngle1"))) {
        m_bottomAngle1 = qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle1")).toDouble(60.0), 174.0);
        m_bottomAngle2 =
            qBound<qreal>(1.0, json.value(QStringLiteral("bottomAngle2")).toDouble(60.0), 175.0 - m_bottomAngle1);
        return;
    }
    // 旧文件没有角度字段：按原三角形的顶点位置（内容矩形上边中点）反算两个底角，
    // 使角度与恢复后的尺寸自洽，外观零变化。
    const QSizeF size = ContentSize();
    const qreal halfWidth = qMax<qreal>(1.0, size.width() / 2.0);
    const qreal legacyAngle = qRadiansToDegrees(qAtan2(qMax<qreal>(1.0, size.height()), halfWidth));
    m_bottomAngle1 = qBound<qreal>(1.0, legacyAngle, 174.0);
    m_bottomAngle2 = qBound<qreal>(1.0, legacyAngle, 175.0 - m_bottomAngle1);
}

void TriangleItem::DrawSelectionOverlay(QPainter* painter)
{
    CanvasItem::DrawSelectionOverlay(painter);
    if (IsCustomShape() || !isSelected() || !IsEditable() || !HasSingleSelection()) {
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
    if (AngleReadoutVisible()) {
        const qreal scale = qMax<qreal>(ViewScale(), 0.001);
        QFont font = painter->font();
        font.setPointSizeF(qMax<qreal>(8.0, 11.0 / scale));
        painter->setFont(font);
        painter->setPen(QColor(37, 99, 235));
        const QFontMetricsF metrics(font);
        const QPointF a = triangleVertex(TriangleVertex::A);
        const QPointF b = triangleVertex(TriangleVertex::B);
        const QString textA = QStringLiteral("%1°").arg(qRound(m_bottomAngle1));
        const QString textB = QStringLiteral("%1°").arg(m_bottomAngle2, 0, 'f', 1);
        painter->drawText(a + QPointF(6.0 / scale, -6.0 / scale), textA);
        painter->drawText(b + QPointF(-metrics.horizontalAdvance(textB) - 6.0 / scale, -6.0 / scale), textB);
    }
    painter->restore();
}
