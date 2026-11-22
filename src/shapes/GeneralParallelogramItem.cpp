#include "shapes/GeneralParallelogramItem.h"

#include "ShapePathBuilder.h"

#include <QTransform>
#include <QtMath>

namespace {
qreal AbsoluteShiftFor(qreal height, qreal angle)
{
    // 上边相对左下顶点（钝角时相对右下顶点）的水平偏移。
    return qAbs(height / qTan(qDegreesToRadians(angle)));
}
} // namespace

GeneralParallelogramItem::GeneralParallelogramItem()
{
    SetContentSize(DefaultContentSize());
}

QSizeF GeneralParallelogramItem::DefaultContentSize() const
{
    // 底边固定为 2 格棋盘格（需求 16 起 1 格 = 64px，2 格 = 128px），高也取 2 格；
    // 60° 时上边右移 128·cot60，内容框宽度 = 底边 + 上边偏移。
    constexpr qreal BASE_LENGTH = 128.0;
    constexpr qreal HEIGHT = 128.0;
    const qreal shift = BASE_LENGTH / qTan(qDegreesToRadians(60.0));
    return QSizeF(BASE_LENGTH + shift, HEIGHT);
}

ItemPropertySet GeneralParallelogramItem::Properties() const
{
    // 内角 A 属于本叶子：矩形不开放（锁 90°），菱形自己上报派生量。
    ItemPropertySet result = ShapeAbstractItem::Properties();
    result.Insert(ItemProperty::PARALLELOGRAM_ANGLE);
    return result;
}

QPainterPath GeneralParallelogramItem::ShapePath(const QRectF& rect) const
{
    return ShapePathBuilder::ParallelogramPath(rect, ParallelogramAngle());
}

void GeneralParallelogramItem::SetParallelogramAngle(qreal angle)
{
    // 自定义轮廓没有平行四边形语义，走族默认只改字段。
    if (IsCustomShape()) {
        ParallelogramAbstractItem::SetParallelogramAngle(angle);
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

    ParallelogramAbstractItem::SetParallelogramAngle(clamped);

    const qreal newShift = AbsoluteShiftFor(oldRect.height(), clamped);
    SetContentSize(QSizeF(baseLength + newShift, oldRect.height()));
    const QRectF newRect = ContentRect();
    const qreal cotNew = 1.0 / qTan(qDegreesToRadians(clamped));
    const qreal newBottomLeftX = cotNew >= 0.0 ? newRect.left() : newRect.left() + newShift;
    // 左下顶点保持不动：包围框为容纳上边向左/右伸出的部分而移动时，图元位置反向补偿。
    const QPointF newBottomLeftScene = mapToScene(QPointF(newBottomLeftX, newRect.bottom()));
    setPos(pos() + (oldBottomLeftScene - newBottomLeftScene));
}
