#include "shapes/ParallelogramItem.h"

#include "ShapePathBuilder.h"

#include <QPainter>
#include <QtMath>

ParallelogramItem::ParallelogramItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF ParallelogramItem::DefaultContentSize() const
{
    return QSizeF(110.85, 64.0);
}

ItemPropertySet ParallelogramItem::Properties() const
{
    ItemPropertySet result = ShapeItem::Properties();
    result.Insert(ItemProperty::PARALLELOGRAM_ANGLE);
    return result;
}

QPainterPath ParallelogramItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::ParallelogramPath(rect, ParallelogramAngle());
}

void ParallelogramItem::SetParallelogramAngle(qreal angle)
{
    ShapeItem::SetParallelogramAngle(angle);
}

QPointF ParallelogramItem::CustomHandleCenter(int handle, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (handle != CanvasItem::APEX || IsCustomShape()) {
        return QPointF();
    }
    // 左上顶点：锐角时上边右移，钝角时贴住左上角；与 parallelogramPath 同一套算法。
    const QRectF rect = ContentRect();
    const qreal cotValue = 1.0 / qTan(qDegreesToRadians(ParallelogramAngle()));
    const qreal shift = qMin(qAbs(rect.height() * cotValue), qMax<qreal>(0.0, rect.width() - 1.0));
    if (ok) {
        *ok = true;
    }
    return QPointF(cotValue >= 0.0 ? rect.left() + shift : rect.left(), rect.top());
}

void ParallelogramItem::DrawSelectionOverlay(QPainter* painter)
{
    ShapeItem::DrawSelectionOverlay(painter);
    if (!AngleReadoutVisible() || !isSelected() || IsCustomShape()) {
        return;
    }
    bool ok = false;
    const QPointF apex = CustomHandleCenter(CanvasItem::APEX, &ok);
    if (!ok) {
        return;
    }
    // 顶点外侧 12px 的只读角度浮层：只随拖动显示，不进状态与撤销。
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    painter->save();
    painter->setPen(QColor(37, 99, 235));
    QFont font = painter->font();
    font.setPointSizeF(qMax<qreal>(8.0, 11.0 / scale));
    painter->setFont(font);
    painter->drawText(apex + QPointF(2.0 / scale, -12.0 / scale),
                      QStringLiteral("%1°").arg(ParallelogramAngle(), 0, 'f', 1));
    painter->restore();
}
