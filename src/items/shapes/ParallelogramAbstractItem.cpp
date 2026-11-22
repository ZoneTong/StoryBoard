#include "items/shapes/ParallelogramAbstractItem.h"

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

void ParallelogramAbstractItem::FrameResizeDragged()
{
    ShapeAbstractItem::FrameResizeDragged();
    // §3.2：菱形拖外接框 / 边时显示派生内角读数（1 位小数；Shift 等比缩放时角度不变）。
    if (shapeType() == ShapeType::DIAMOND && !IsCustomShape()) {
        SetAngleReadoutVisible(true);
    }
}

void ParallelogramAbstractItem::WriteState(ShapeItemState& state) const
{
    // 先让基类按属性表写公共标量（圆角等），再补本族的内角；
    // 漏掉基类会让矩形的圆角整行既不进快照也不回放。
    ShapeAbstractItem::WriteState(state);
    // 内角 A 按 ItemProperty 进 propertyExtras：普通平行四边形写字段真值，
    // 菱形同样写字段（菱形的尺寸才是真相，派生角由宽高还原）。
    if (Properties().Contains(ItemProperty::PARALLELOGRAM_ANGLE)) {
        state.propertyExtras.insert(ItemProperty::PARALLELOGRAM_ANGLE, m_parallelogramAngle);
    }
}

void ParallelogramAbstractItem::ApplyState(const ShapeItemState& state)
{
    // 先写角度再走基类：基类按状态里的尺寸 / 位置整体恢复，这里不带叶子副作用。
    const qreal target = state.propertyExtras.value(ItemProperty::PARALLELOGRAM_ANGLE, m_parallelogramAngle).toDouble();
    ApplyParallelogramAngleRaw(qBound<qreal>(5.0, target, 175.0));
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
