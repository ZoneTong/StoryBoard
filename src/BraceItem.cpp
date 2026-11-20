#include "BraceItem.h"

#include "ShapePathBuilder.h"

#include <QPainterPath>
#include <QVector>

namespace {

// 采样段数：与图标和插入端此前共用的一致，弯折处足够平滑。
constexpr int BRACE_SAMPLES = 64;

} // namespace

ItemPropertySet BraceItem::properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineItemAbstract::properties().united(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant BraceItem::propertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(startArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(endArrow());
    default:
        return LineItemAbstract::propertyValue(property);
    }
}

void BraceItem::setPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::ARROW_START:
        setArrowStyles(static_cast<ArrowStyle>(value.toInt()), endArrow());
        break;
    case ItemProperty::ARROW_END:
        setArrowStyles(startArrow(), static_cast<ArrowStyle>(value.toInt()));
        break;
    default:
        LineItemAbstract::setPropertyValue(property, value);
        break;
    }
}

BraceItem::BraceItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

void BraceItem::setBraceRect(const QRectF& rectScene, const QColor& color, qreal width)
{
    const QRectF frame = rectScene.normalized();
    const QPainterPath bracePath = ShapePathBuilder::bracePath(QRectF(QPointF(0.0, 0.0), frame.size()));
    QVector<QPointF> points;
    points.reserve(BRACE_SAMPLES + 1);
    for (int i = 0; i <= BRACE_SAMPLES; ++i) {
        points.append(frame.topLeft() + bracePath.pointAtPercent(qreal(i) / qreal(BRACE_SAMPLES)));
    }
    setStrokePoints(points, color, width, 0.0, true);
}
