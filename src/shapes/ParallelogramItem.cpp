#include "shapes/ParallelogramItem.h"

#include "ShapePathBuilder.h"

#include <QPainter>
#include <QTransform>
#include <QtMath>

namespace {
qreal AbsoluteShiftFor(qreal height, qreal angle)
{
    // 上边相对左下顶点（钝角时相对右下顶点）的水平偏移。
    return qAbs(height / qTan(qDegreesToRadians(angle)));
}
} // namespace

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
    // 菱形只是复用这个字段保存 90°，轮廓由宽高决定；不能按平行四边形改尺寸。
    if (shapeType() != ShapeType::PARALLELOGRAM || IsCustomShape()) {
        ShapeItem::SetParallelogramAngle(angle);
        return;
    }

    const qreal oldAngle = ParallelogramAngle();
    const qreal clamped = qBound<qreal>(5.0, angle, 175.0);
    if (qFuzzyCompare(oldAngle + 2.0, clamped + 2.0)) {
        return;
    }

    // 角度改变时只让上边绕底边旋转：底边长度保持不变，包围框随偏移量增减。
    // 旧底边 = 旧框宽 − 旧上边偏移；新框宽 = 旧底边 + 新上边偏移。
    const QRectF oldRect = ContentRect();
    const qreal cotOld = 1.0 / qTan(qDegreesToRadians(oldAngle));
    const qreal oldShift = qMin(AbsoluteShiftFor(oldRect.height(), oldAngle), qMax<qreal>(0.0, oldRect.width() - 1.0));
    const qreal baseLength = qMax<qreal>(1.0, oldRect.width() - oldShift);
    const qreal oldBottomLeftX = cotOld >= 0.0 ? oldRect.left() : oldRect.left() + oldShift;
    const QPointF oldBottomLeftScene = mapToScene(QPointF(oldBottomLeftX, oldRect.bottom()));

    ShapeItem::SetParallelogramAngle(clamped);

    const qreal newShift = AbsoluteShiftFor(oldRect.height(), clamped);
    SetContentSize(QSizeF(baseLength + newShift, oldRect.height()));
    const QRectF newRect = ContentRect();
    const qreal cotNew = 1.0 / qTan(qDegreesToRadians(clamped));
    const qreal newBottomLeftX = cotNew >= 0.0 ? newRect.left() : newRect.left() + newShift;
    // 左下顶点保持不动：包围框为容纳上边向左/右伸出的部分而移动时，图元位置反向补偿。
    const QPointF newBottomLeftScene = mapToScene(QPointF(newBottomLeftX, newRect.bottom()));
    setPos(pos() + (oldBottomLeftScene - newBottomLeftScene));
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
    // 往下让开顶边中点（Top 缩放句柄），并保证内角调到 90° 时旋钮也不与顶点/角手柄
    // 重合；形很矮时收在内框里。
    const qreal inset = qMin<qreal>(22.0 / qMax<qreal>(ViewScale(), 0.001),
                                    qMax<qreal>(0.0, rect.height() - 7.0 / qMax<qreal>(ViewScale(), 0.001)));
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
