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
    if (handle != CanvasItem::SKEW || IsCustomShape()) {
        return QPointF();
    }
    // 上边中点的倾斜旋钮：上边中点相对内容框中心的水平偏移正好是有符号偏移的一半。
    // 上边偏移 = 高 × cot(角度)，锐角为正（上边右移）、钝角为负（上边左移），
    // 旋钮因此能连续跨过 90°（正负交界处落在框中心）。
    const QRectF rect = ContentRect();
    const qreal cotValue = 1.0 / qTan(qDegreesToRadians(ParallelogramAngle()));
    qreal shift = qAbs(rect.height() * cotValue);
    shift = qMin(shift, qMax<qreal>(0.0, rect.width() - 1.0));
    const qreal signedShift = cotValue >= 0.0 ? shift : -shift;
    // 往下让开顶边中点（Top 缩放句柄）半个句柄加间隙；形很矮时收在内框里。
    const qreal inset = qMin(qMax<qreal>(0.0, rect.height() * 0.5), 14.0 / qMax<qreal>(ViewScale(), 0.001));
    if (ok) {
        *ok = true;
    }
    return QPointF(rect.center().x() + signedShift / 2.0, rect.top() + inset);
}

void ParallelogramItem::DrawSelectionOverlay(QPainter* painter)
{
    ShapeItem::DrawSelectionOverlay(painter);
    if (!AngleReadoutVisible() || !isSelected() || IsCustomShape()) {
        return;
    }
    bool ok = false;
    const QPointF handle = CustomHandleCenter(CanvasItem::SKEW, &ok);
    if (!ok) {
        return;
    }
    // 旋钮外侧 12px 的只读角度浮层：只随拖动显示，不进状态与撤销。
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    painter->save();
    painter->setPen(QColor(37, 99, 235));
    QFont font = painter->font();
    font.setPointSizeF(qMax<qreal>(8.0, 11.0 / scale));
    painter->setFont(font);
    painter->drawText(handle + QPointF(10.0 / scale, 4.0 / scale),
                      QStringLiteral("%1°").arg(ParallelogramAngle(), 0, 'f', 1));
    painter->restore();
}
