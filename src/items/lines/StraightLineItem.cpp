#include "items/lines/StraightLineItem.h"

#include "items/CanvasItem.h"
#include "scene/CanvasScene.h"

#include <QJsonObject>
#include <QVector>

namespace {

constexpr qreal MINIMUM_FRAME_SIDE = 12.0;

QRectF PaddedToMinimum(QRectF frame)
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

StraightLineItem::StraightLineItem(QGraphicsItem* parent) : LineAbstractItem(parent) {}

ItemPropertySet StraightLineItem::Properties() const
{
    // 箭头行对全部描线子型开放（含花括号），与现状一致。
    return LineAbstractItem::Properties().United(ItemPropertySet{ItemProperty::ARROW_START, ItemProperty::ARROW_END});
}

QVariant StraightLineItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    default:
        return LineAbstractItem::PropertyValue(property);
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
        LineAbstractItem::SetPropertyValue(property, value);
        break;
    }
}

QString StraightLineItem::DisplayName() const
{
    return ArrowStyleIsHead(StartArrow()) || ArrowStyleIsHead(EndArrow()) ? UiText(QStringLiteral("Line Arrow"))
                                                                          : UiText(QStringLiteral("Straight Line"));
}

QRectF StraightLineItem::FrameRect() const
{
    return PaddedToMinimum(CanvasItem::FrameRect());
}

void StraightLineItem::SetEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color,
                                    qreal width)
{
    SetStrokePoints(QVector<QPointF>{startScene, endScene}, color, width, 0.0, true);
}

HandleSpecList StraightLineItem::CustomHandles() const
{
    HandleSpecList specs;
    const QVector<QPointF> knots = StrokePoints();
    if (knots.size() < 2) {
        return specs;
    }
    HandleSpec start;
    start.id = CanvasItem::CUSTOM_0;
    start.position = knots.constFirst();
    start.behavior = HandleBehavior::ARC_ENDPOINT;
    start.cursor = Qt::SizeAllCursor;
    start.sizeScale = 0.9;
    specs.append(start);
    HandleSpec end;
    end.id = CanvasItem::CUSTOM_1;
    end.position = knots.constLast();
    end.behavior = HandleBehavior::ARC_ENDPOINT;
    end.cursor = Qt::SizeAllCursor;
    end.sizeScale = 0.9;
    specs.append(end);
    return specs;
}

bool StraightLineItem::BeginCustomHandleDrag(int handle, const QPointF& scenePos)
{
    Q_UNUSED(scenePos);
    return handle == CanvasItem::CUSTOM_0 || handle == CanvasItem::CUSTOM_1;
}

bool StraightLineItem::ApplyCustomHandleDrag(int handle, const QPointF& scenePos)
{
    if (handle != CanvasItem::CUSTOM_0 && handle != CanvasItem::CUSTOM_1) {
        return false;
    }
    const bool start = handle == CanvasItem::CUSTOM_0;
    MoveEndpoint(start, scenePos);

    CanvasScene* canvas = canvasScene();
    if (!canvas) {
        return true;
    }
    // Excalidraw 口径：拖动端点靠近 / 落进图形就绑上，拖离超过 12px 就解绑。
    CanvasItem* target = canvas->BindableShapeAt(scenePos, 12.0);
    if (target && target->Category() == ItemCategory::SHAPE && target->Uid() != Uid()) {
        QJsonObject binding;
        binding.insert(QStringLiteral("elementId"), target->Uid());
        binding.insert(QStringLiteral("focus"), 0.0);
        binding.insert(QStringLiteral("gap"), 4.0);
        const QVector<QPointF> knots = StrokePoints();
        const QPointF other = mapToScene(start ? knots.at(1) : knots.at(qMax(1, knots.size() - 2)));
        MoveEndpoint(start, CanvasScene::AttachmentPoint(target, other, 4.0));
        if (start) {
            SetStartBinding(binding);
        } else {
            SetEndBinding(binding);
        }
    } else {
        if (start) {
            SetStartBinding(QJsonObject());
        } else {
            SetEndBinding(QJsonObject());
        }
    }
    return true;
}
