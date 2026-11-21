#include "StraightLineItem.h"

#include <QVector>

namespace {

constexpr qreal MINIMUM_FRAME_SIDE = 12.0;

QRectF paddedToMinimum(QRectF frame)
{
    if (frame.width() < MINIMUM_FRAME_SIDE) {
        frame.moveLeft(frame.center().x() - MINIMUM_FRAME_SIDE / 2.0);
        frame.setWidth(MINIMUM_FRAME_SIDE);
    }
    if (frame.height() < MINIMUM_FRAME_SIDE) {
        frame.moveTop(frame.center().y() - MINIMUM_FRAME_SIDE / 2.0);
        frame.setHeight(MINIMUM_FRAME_SIDE);
    }
    return frame;
}

} // namespace

StraightLineItem::StraightLineItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

ItemPropertySet StraightLineItem::Properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineItemAbstract::Properties().united(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant StraightLineItem::PropertyValue(ItemProperty property) const
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

void StraightLineItem::SetPropertyValue(ItemProperty property, const QVariant& value)
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

QString StraightLineItem::DisplayName() const
{
    return arrowStyleIsHead(StartArrow()) || arrowStyleIsHead(EndArrow()) ? uiText(QStringLiteral("Line Arrow"))
                                                                          : uiText(QStringLiteral("Straight Line"));
}

QRectF StraightLineItem::FrameRect() const
{
    return paddedToMinimum(CanvasItem::FrameRect());
}

void StraightLineItem::setEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color,
                                    qreal width)
{
    SetStrokePoints(QVector<QPointF>{startScene, endScene}, color, width, 0.0, true);
}
