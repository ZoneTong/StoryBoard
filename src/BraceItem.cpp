#include "BraceItem.h"

#include "ShapePathBuilder.h"

#include <QPainterPath>
#include <QVector>

namespace {

// 采样段数：与图标和插入端此前共用的一致，弯折处足够平滑。
constexpr int BRACE_SAMPLES = 64;

} // namespace

ItemPropertySet BraceItem::Properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineItemAbstract::Properties().United(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant BraceItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    default:
        return LineItemAbstract::PropertyValue(property);
    }
}

void BraceItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::ARROW_START:
        SetArrowStyles(static_cast<ArrowStyle>(value.toInt()), EndArrow());
        break;
    case ItemProperty::ARROW_END:
        SetArrowStyles(StartArrow(), static_cast<ArrowStyle>(value.toInt()));
        break;
    default:
        LineItemAbstract::SetPropertyValue(property, value);
        break;
    }
}

BraceItem::BraceItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

void BraceItem::SetBraceRect(const QRectF& rectScene, const QColor& color, qreal width)
{
    const QRectF frame = rectScene.normalized();
    const QPainterPath bracePath = ShapePathBuilder::BracePath(QRectF(QPointF(0.0, 0.0), frame.size()));
    QVector<QPointF> points;
    points.reserve(BRACE_SAMPLES + 1);
    for (int i = 0; i <= BRACE_SAMPLES; ++i) {
        points.append(frame.topLeft() + bracePath.pointAtPercent(qreal(i) / qreal(BRACE_SAMPLES)));
    }
    SetStrokePoints(points, color, width, 0.0, true);
}
