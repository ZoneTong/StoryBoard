#include "shapes/ParallelogramAbstractItem.h"

#include <QJsonObject>
#include <QPainter>
#include <QtMath>

void ParallelogramAbstractItem::SetParallelogramAngle(qreal angle)
{
    const qreal clamped = qBound<qreal>(5.0, angle, 175.0);
    if (qFuzzyCompare(m_parallelogramAngle + 2.0, clamped + 2.0)) {
        return;
    }
    m_parallelogramAngle = clamped;
    // 族默认只改字段：普通平行四边形覆写后连带改包围框，矩形锁 90°，菱形按宽反算高。
    prepareGeometryChange();
    update();
}

QPointF ParallelogramAbstractItem::BottomLeftVertex() const
{
    const QRectF rect = ContentRect();
    const qreal cotValue = 1.0 / qTan(qDegreesToRadians(ParallelogramAngle()));
    qreal shift = qAbs(rect.height() * cotValue);
    shift = qMin(shift, qMax<qreal>(0.0, rect.width() - 1.0));
    // 锐角：上边右移，左下顶点就是内容框左下角；钝角：上边左伸，顶点退到框内。
    const qreal x = cotValue >= 0.0 ? rect.left() : rect.left() + shift;
    return QPointF(x, rect.bottom());
}

QVariant ParallelogramAbstractItem::PropertyValue(ItemProperty property) const
{
    if (property == ItemProperty::PARALLELOGRAM_ANGLE) {
        return ParallelogramAngle();
    }
    return ShapeAbstractItem::PropertyValue(property);
}

void ParallelogramAbstractItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    if (property == ItemProperty::PARALLELOGRAM_ANGLE) {
        SetParallelogramAngle(value.toDouble());
        return;
    }
    ShapeAbstractItem::SetPropertyValue(property, value);
}

QPointF ParallelogramAbstractItem::CustomHandleCenter(int handle, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (handle != CanvasItem::SKEW || !SkewHandleEnabled() || IsCustomShape()) {
        return CanvasItem::CustomHandleCenter(handle, ok);
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

ShapeItemState ParallelogramAbstractItem::State() const
{
    ShapeItemState state = ShapeAbstractItem::State();
    state.parallelogramAngle = m_parallelogramAngle;
    return state;
}

void ParallelogramAbstractItem::ApplyState(const ShapeItemState& state)
{
    // 先写角度再走基类：基类按状态里的尺寸 / 位置整体恢复，这里不带叶子副作用。
    ApplyParallelogramAngleRaw(qBound<qreal>(5.0, state.parallelogramAngle, 175.0));
    ShapeAbstractItem::ApplyState(state);
}

QPointF ParallelogramAbstractItem::AngleReadoutAnchor() const
{
    return BottomLeftVertex();
}

void ParallelogramAbstractItem::DrawSelectionOverlay(QPainter* painter)
{
    ShapeAbstractItem::DrawSelectionOverlay(painter);
    if (!AngleReadoutVisible() || !isSelected() || IsCustomShape()) {
        return;
    }
    // 角度浮层贴角度锚点（§3.2：普通平行四边形为左下顶点，不再贴 SKEW 旋钮）：
    // 只读浮层，只随拖动显示，不进状态与撤销。
    const QPointF anchor = AngleReadoutAnchor();
    const qreal scale = qMax<qreal>(ViewScale(), 0.001);
    painter->save();
    painter->setPen(QColor(37, 99, 235));
    QFont font = painter->font();
    font.setPointSizeF(qMax<qreal>(8.0, 11.0 / scale));
    painter->setFont(font);
    painter->drawText(anchor + QPointF(10.0 / scale, 4.0 / scale),
                      QStringLiteral("%1°").arg(ParallelogramAngle(), 0, 'f', 1));
    painter->restore();
}

void ParallelogramAbstractItem::WriteJsonExtra(QJsonObject& json) const
{
    // 只有普通平行四边形把内角当落盘真相；矩形锁 90°，菱形由宽高派生，都不写。
    if (shapeType() == ShapeType::PARALLELOGRAM) {
        json.insert(QStringLiteral("parallelogramAngle"), m_parallelogramAngle);
    }
}

void ParallelogramAbstractItem::ReadJsonExtra(const QJsonObject& json)
{
    if (shapeType() == ShapeType::PARALLELOGRAM) {
        m_parallelogramAngle =
            qBound<qreal>(15.0, json.value(QStringLiteral("parallelogramAngle")).toDouble(60.0), 165.0);
    }
}
