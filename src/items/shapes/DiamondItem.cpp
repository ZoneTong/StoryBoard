#include "items/shapes/DiamondItem.h"

#include "items/geometry/ShapePathBuilder.h"

#include <QtMath>

DiamondItem::DiamondItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF DiamondItem::DefaultContentSize() const
{
    // 横对角线 128、竖对角线 72：插入即“一条对角线水平”，与 Excalidraw 的
    // diamond 外接框语义一致（宽即水平对角线）。
    return QSizeF(128.0, 72.0);
}

ItemPropertySet DiamondItem::Properties() const
{
    // §3.2 菱形复用内角 A：行由属性表驱动，写入侧按宽反算高。
    ItemPropertySet result = ShapeAbstractItem::Properties();
    result.Insert(ItemProperty::PARALLELOGRAM_ANGLE);
    return result;
}

qreal DiamondItem::ParallelogramAngle() const
{
    const QSizeF size = ContentSize();
    return qRadiansToDegrees(2.0 * qAtan2(size.height(), qMax<qreal>(1.0, size.width())));
}

void DiamondItem::SetParallelogramAngle(qreal angle)
{
    // 自定义轮廓没有菱形语义，走族默认只改字段。
    if (IsCustomShape()) {
        ParallelogramAbstractItem::SetParallelogramAngle(angle);
        return;
    }
    const qreal clamped = qBound<qreal>(5.0, qRound(angle), 175.0);
    const QRectF rect = ContentRect();
    // h = w·tan(A/2)：宽保持不动，只由内角反算高；中心保持不动。
    const qreal newHeight = qMax<qreal>(1.0, rect.width() * qTan(qDegreesToRadians(clamped / 2.0)));
    if (qFuzzyCompare(rect.height() + 2.0, newHeight + 2.0)) {
        return;
    }
    const QPointF centerScene = mapToScene(rect.center());
    m_syncingDiamond = true;
    SetContentSize(QSizeF(rect.width(), newHeight));
    m_syncingDiamond = false;
    const QPointF newCenterScene = mapToScene(ContentRect().center());
    setPos(pos() + (centerScene - newCenterScene));
}

void DiamondItem::ContentResized()
{
    if (IsCustomShape() || m_syncingDiamond) {
        return;
    }
    // 四角拖动自由缩放；只有非四角边拖动才把派生内角吸附到整数度。
    const Handle handle = ActiveResizeHandle();
    const bool horizontalSide = handle == CanvasItem::LEFT || handle == CanvasItem::RIGHT;
    const bool verticalSide = handle == CanvasItem::TOP || handle == CanvasItem::BOTTOM;
    if (!horizontalSide && !verticalSide) {
        return;
    }
    const QRectF rect = ContentRect();
    qreal angle = qRound(ParallelogramAngle());
    angle = qBound<qreal>(5.0, angle, 175.0);
    const qreal tangent = qTan(qDegreesToRadians(angle / 2.0));
    QSizeF snapped = rect.size();
    if (horizontalSide && tangent > 0.001) {
        snapped.setWidth(qMax<qreal>(8.0, rect.height() / tangent));
    } else if (verticalSide) {
        snapped.setHeight(qMax<qreal>(8.0, rect.width() * tangent));
    }
    if (qFuzzyCompare(snapped.width() + 0.1, rect.width() + 0.1) &&
        qFuzzyCompare(snapped.height() + 0.1, rect.height() + 0.1)) {
        return;
    }
    m_syncingDiamond = true;
    SetContentSize(snapped);
    m_syncingDiamond = false;
}

void DiamondItem::ApplyStateContentSize(const QSizeF& requested)
{
    // 状态回放保留文件里的宽高，不把历史文档吸附到新的整数角度。
    m_syncingDiamond = true;
    ShapeAbstractItem::ApplyStateContentSize(requested);
    m_syncingDiamond = false;
}

QPointF DiamondItem::AngleReadoutAnchor() const
{
    const QRectF rect = ContentRect();
    // 左顶点：读数贴着被展示的角本身。
    return QPointF(rect.left(), rect.center().y());
}

QPainterPath DiamondItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::DiamondPath(rect);
}
