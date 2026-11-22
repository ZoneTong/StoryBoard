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
    const qreal clamped = qBound<qreal>(5.0, angle, 175.0);
    const QRectF rect = ContentRect();
    // h = w·tan(A/2)：宽保持不动，只由内角反算高；中心保持不动。
    const qreal newHeight = qMax<qreal>(1.0, rect.width() * qTan(qDegreesToRadians(clamped / 2.0)));
    if (qFuzzyCompare(rect.height() + 2.0, newHeight + 2.0)) {
        return;
    }
    const QPointF centerScene = mapToScene(rect.center());
    SetContentSize(QSizeF(rect.width(), newHeight));
    const QPointF newCenterScene = mapToScene(ContentRect().center());
    setPos(pos() + (centerScene - newCenterScene));
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
