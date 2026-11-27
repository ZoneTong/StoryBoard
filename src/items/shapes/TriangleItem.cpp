#include "items/shapes/TriangleItem.h"

#include "items/CanvasItem.h"
#include "items/geometry/ShapePathBuilder.h"
#include "items/geometry/TriangleGeometry.h"
#include "items/geometry/VertexAngleKnob.h"

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
    // 与平行四边形 / 菱形同一口径：旋钮贴在要改的那个角（底角 A）的顶点内侧平分线上，
    // 绕顶点拖动改 A。旋钮不再压在半高处，Top 缩放句柄保留。
    const QRectF rect = ContentRect();
    const QPointF vertexA = triangleVertex(TriangleVertex::A);
    HandleSpec apex;
    apex.id = CanvasItem::CUSTOM_0;
    apex.position = VertexAngleKnob::Position(vertexA, triangleVertex(TriangleVertex::B),
                                              triangleVertex(TriangleVertex::C), ViewScale(), rect);
    apex.behavior = HandleBehavior::ANGLE;
    apex.cursor = Qt::SizeHorCursor;
    apex.hitPadding = 16.0;
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
    // 底角 A 旋钮只属于三角形；自定义轮廓没有角度语义，不提供专用拖拽。
    if (handle != CanvasItem::CUSTOM_0 || IsCustomShape()) {
        return false;
    }
    m_apexDragBaseA = mapToScene(triangleVertex(TriangleVertex::A));
    m_apexDragBaseB = mapToScene(triangleVertex(TriangleVertex::B));
    m_apexDragHeight = qMax<qreal>(1.0, ContentSize().height());
    m_apexDragStartAngle = m_bottomAngle1;
    m_apexDragStartPointer = VertexAngleKnob::PointerAngleDegrees(m_apexDragBaseA, scenePos, RotationAngle());
    SetAngleReadoutVisible(true);
    return true;
}

QString TriangleItem::CustomHandleActionText(int handle) const
{
    Q_UNUSED(handle);
    return CanvasItem::tr("Adjust inner angle");
}

bool TriangleItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 || IsCustomShape()) {
        return false;
    }
    // §3.1：指针绕左下顶点 A 转多少度，底角 A 就变多少度；AB 边上的垂直高度保持
    // 拖拽开始时的值，B 由底边长度与高度反算（cotB = base/h − cotA），按 1° 取整。
    const QPointF base = m_apexDragBaseB - m_apexDragBaseA;
    const qreal baseLength = qMax<qreal>(1.0, std::hypot(base.x(), base.y()));
    const qreal pointer = VertexAngleKnob::PointerAngleDegrees(m_apexDragBaseA, scenePos, RotationAngle());
    const qreal angleA = qRound(m_apexDragStartAngle + (pointer - m_apexDragStartPointer));
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
    if (!AngleReadoutVisible()) {
        return;
    }
    // 两个底角各挂一张读数底片（白底圆角 + 蓝字），贴着各自的顶点，
    // 与平行四边形 / 菱形的角度读数同一份口径。
    const QList<QRectF> chips = AngleReadoutRects();
    const QString textA = QStringLiteral("%1°").arg(qRound(m_bottomAngle1));
    const QString textB = QStringLiteral("%1°").arg(qRound(m_bottomAngle2));
    if (chips.size() == 2) {
        DrawReadoutChip(painter, chips.constFirst(), textA);
        DrawReadoutChip(painter, chips.constLast(), textB);
    }
}

QList<QRectF> TriangleItem::AngleReadoutRects() const
{
    if (!AngleReadoutVisible() || IsCustomShape()) {
        return {};
    }
    const QString textA = QStringLiteral("%1°").arg(qRound(m_bottomAngle1));
    const QString textB = QStringLiteral("%1°").arg(qRound(m_bottomAngle2));
    // A 在左下顶点往角内侧展开，B 在右下顶点反向展开。
    return {ReadoutChipRect(triangleVertex(TriangleVertex::A), textA, true),
            ReadoutChipRect(triangleVertex(TriangleVertex::B), textB, false)};
}
