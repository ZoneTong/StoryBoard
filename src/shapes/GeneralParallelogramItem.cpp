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

HandleSpecList GeneralParallelogramItem::CustomHandles() const
{
    HandleSpecList specs;
    if (IsCustomShape()) {
        return specs;
    }
    // 上边中点相对内容框中心的水平偏移正好是有符号偏移的一半：上边偏移 = 高 × cot(角度)，
    // 锐角为正（上边右移）、钝角为负（上边左移），旋钮因此能连续跨过 90°（交界落在框中心）。
    const QRectF rect = ContentRect();
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    const qreal cotValue = 1.0 / qTan(qDegreesToRadians(ParallelogramAngle()));
    qreal shift = qAbs(rect.height() * cotValue);
    shift = qMin(shift, qMax<qreal>(0.0, rect.width() - 1.0));
    const qreal signedShift = cotValue >= 0.0 ? shift : -shift;
    // 往下让开顶边中点（Top 缩放句柄），并保证内角调到 90° 时旋钮也不与顶点 / 角手柄
    // 重合；形很矮时收在内框里。
    const qreal inset = qMin<qreal>(22.0 / scale, qMax<qreal>(0.0, rect.height() - 7.0 / scale));
    HandleSpec knob;
    knob.id = CanvasItem::CUSTOM_1;
    knob.position = QPointF(rect.center().x() + signedShift / 2.0, rect.top() + inset);
    knob.behavior = HandleBehavior::SKEW;
    knob.cursor = Qt::SizeHorCursor;
    specs.append(knob);
    return specs;
}

bool GeneralParallelogramItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    if (handle != CanvasItem::CUSTOM_1 || IsCustomShape()) {
        return false;
    }
    // 内角旋钮绕左下顶点旋转上边：先记住底边的场景锚点与长度。
    // 拖动中包围框会随上边偏移量增减，不能再用固定的拖拽起点推算。
    const QRectF rect = ContentRect();
    const qreal cotValue = 1.0 / qTan(qDegreesToRadians(ParallelogramAngle()));
    const qreal shift = qMin(qAbs(rect.height() * cotValue), qMax<qreal>(0.0, rect.width() - 1.0));
    m_skewDragBaseLength = qMax<qreal>(1.0, rect.width() - shift);
    const qreal bottomLeftX = cotValue >= 0.0 ? rect.left() : rect.left() + shift;
    m_skewDragBottomLeft = mapToScene(QPointF(bottomLeftX, rect.bottom()));
    return true;
}

bool GeneralParallelogramItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_1 || IsCustomShape()) {
        return false;
    }
    // 旋钮位于“底边中点 + 上边偏移”，以固定左下顶点为原点量横向距离：
    // 上边偏移 = 指针到底边中点的距离 − 底边半长。上边向左右移动即改变左下内角，
    // 底边长度全程保持不动。
    const qreal startHeight = ContentSize().height();
    QPointF along(1.0, 0.0);
    const qreal rotation = RotationAngle();
    if (!qFuzzyIsNull(rotation)) {
        QTransform transform;
        transform.rotate(rotation);
        along = transform.map(along);
    }
    const qreal signedShift = QPointF::dotProduct(scenePos - m_skewDragBottomLeft, along) - m_skewDragBaseLength / 2.0;
    SetParallelogramAngle(qRadiansToDegrees(qAtan2(startHeight, signedShift)));
    SetAngleReadoutVisible(true);
    return true;
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
