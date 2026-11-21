#include "SineItem.h"

#include <QJsonArray>
#include <QtMath>

namespace {

constexpr qreal MINIMUM_FRAME_SIDE = 12.0;
constexpr int STEPS_PER_CYCLE = 64;

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

QJsonArray RectToArray(const QRectF& rect)
{
    QJsonArray array;
    array.append(rect.x());
    array.append(rect.y());
    array.append(rect.width());
    array.append(rect.height());
    return array;
}

QRectF ArrayToRect(const QJsonArray& array, const QRectF& fallback)
{
    if (array.size() < 4) {
        return fallback;
    }
    return QRectF(array.at(0).toDouble(), array.at(1).toDouble(), array.at(2).toDouble(), array.at(3).toDouble());
}

} // namespace

SineItem::SineItem(QGraphicsItem* parent) : LineItemAbstract(parent) {}

ItemPropertySet SineItem::Properties() const
{
    // 正弦有箭头行，另有周期数与振幅两行参数。
    return LineItemAbstract::Properties().United(ItemPropertySet{
        ItemProperty::ARROW_START, ItemProperty::ARROW_END, ItemProperty::SINE_CYCLES, ItemProperty::SINE_AMPLITUDE});
}

QVariant SineItem::PropertyValue(ItemProperty property) const
{
    switch (property) {
    case ItemProperty::ARROW_START:
        return static_cast<int>(StartArrow());
    case ItemProperty::ARROW_END:
        return static_cast<int>(EndArrow());
    case ItemProperty::SINE_CYCLES:
        return m_params.cycles;
    case ItemProperty::SINE_AMPLITUDE:
        return m_params.amplitude;
    default:
        return LineItemAbstract::PropertyValue(property);
    }
}

void SineItem::SetPropertyValue(ItemProperty property, const QVariant& value)
{
    switch (property) {
    case ItemProperty::ARROW_START:
        SetArrowStyles(static_cast<ArrowStyle>(value.toInt()), EndArrow());
        break;
    case ItemProperty::ARROW_END:
        SetArrowStyles(StartArrow(), static_cast<ArrowStyle>(value.toInt()));
        break;
    case ItemProperty::SINE_CYCLES: {
        Params params = m_params;
        params.cycles = value.toInt();
        SetParams(params);
        break;
    }
    case ItemProperty::SINE_AMPLITUDE: {
        Params params = m_params;
        params.amplitude = value.toDouble();
        SetParams(params);
        break;
    }
    default:
        LineItemAbstract::SetPropertyValue(property, value);
        break;
    }
}

QString SineItem::DisplayName() const
{
    return UiText(QStringLiteral("Sine wave"));
}

QRectF SineItem::FrameRect() const
{
    return PaddedToMinimum(CanvasItem::FrameRect());
}

SineItem::ParamState SineItem::paramState() const
{
    return ParamState{qreal(m_params.cycles), m_params.amplitude, true};
}

void SineItem::ApplyParamState(const ParamState& state)
{
    Params params = m_params;
    params.cycles = qBound(1, qRound(state.first), 12);
    params.amplitude = qBound<qreal>(1.0, state.second, 200.0);
    SetParams(params);
}

void SineItem::SetParams(const Params& params)
{
    const int cycles = qBound(1, params.cycles, 12);
    const qreal amplitude = qBound<qreal>(1.0, params.amplitude, 200.0);
    if (cycles == m_params.cycles && qFuzzyCompare(amplitude, m_params.amplitude)) {
        return;
    }
    m_params.cycles = cycles;
    m_params.amplitude = amplitude;
    Rebuild();
}

QVector<QPointF> SineItem::WavePoints(const QRectF& frameScene) const
{
    QVector<QPointF> points;
    if (frameScene.width() <= 0.0 || frameScene.height() <= 0.0) {
        return points;
    }

    const int steps = STEPS_PER_CYCLE * m_params.cycles;
    // 振幅是相对中心线的像素高度，与内容框高度无关：框高由波高反推，不是振幅的来源。
    const qreal amplitude = m_params.amplitude;
    const qreal centerY = frameScene.center().y();

    points.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const qreal t = qreal(i) / qreal(steps);
        const qreal x = frameScene.left() + t * frameScene.width();
        const qreal y = centerY - qSin(t * 2.0 * M_PI * m_params.cycles) * amplitude;
        points.append(QPointF(x, y));
    }
    return points;
}

void SineItem::SetWaveGeometry(const QPointF& topLeftScene, const QSizeF& size, const QColor& color, qreal width)
{
    const QRectF frameScene(topLeftScene, size);
    // 程序化插入时按给定振幅采样，不让 contentResized 的“按高度反推振幅”插一脚。
    m_rebuilding = true;
    SetStrokePoints(WavePoints(frameScene), color, width, 0.0, false);
    m_rebuilding = false;

    const QPointF origin = pos();
    const QSizeF box = ContentSize();
    if (box.width() > 0.0 && box.height() > 0.0) {
        m_refFrame =
            QRectF((frameScene.left() - origin.x()) / box.width(), (frameScene.top() - origin.y()) / box.height(),
                   frameScene.width() / box.width(), frameScene.height() / box.height());
    } else {
        m_refFrame = QRectF(0.0, 0.0, 1.0, 1.0);
    }
}

void SineItem::ContentResized()
{
    if (m_rebuilding) {
        return;
    }
    // 内容框高度 = 波高 + 笔宽余量；拖高拖矮等价于改振幅，参数跟着几何走，面板回显也随之更新。
    const qreal boxHeight = ContentSize().height();
    if (boxHeight <= 0.0) {
        return;
    }
    const qreal padding = qMax<qreal>(6.0, StrokeWidth() + 4.0);
    const qreal amplitude = qBound<qreal>(1.0, qMax<qreal>(1.0, boxHeight - padding) / 2.0, 200.0);
    if (qAbs(amplitude - m_params.amplitude) < 0.5) {
        return;
    }
    m_params.amplitude = amplitude;
    Rebuild();
}

void SineItem::Rebuild()
{
    const QSizeF box = ContentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_refFrame.x() * box.width(), m_refFrame.y() * box.height(), m_refFrame.width() * box.width(),
                       m_refFrame.height() * box.height());
    const QRectF frameScene = local.translated(pos());
    m_rebuilding = true;
    SetStrokePoints(WavePoints(frameScene), StrokeColor(), StrokeWidth(), 0.0, false);
    m_rebuilding = false;

    const QPointF origin = pos();
    const QSizeF now = ContentSize();
    if (now.width() > 0.0 && now.height() > 0.0) {
        m_refFrame =
            QRectF((frameScene.left() - origin.x()) / now.width(), (frameScene.top() - origin.y()) / now.height(),
                   frameScene.width() / now.width(), frameScene.height() / now.height());
    }
    update();
}

QJsonObject SineItem::ToJson() const
{
    QJsonObject json = LineItemAbstract::ToJson();
    json.insert(QStringLiteral("sineCycles"), m_params.cycles);
    json.insert(QStringLiteral("sineAmplitude"), m_params.amplitude);
    json.insert(QStringLiteral("sineFrame"), RectToArray(m_refFrame));
    return json;
}

void SineItem::FromJson(const QJsonObject& json)
{
    LineItemAbstract::FromJson(json);
    m_params.cycles = qBound(1, json.value(QStringLiteral("sineCycles")).toInt(1), 12);
    m_params.amplitude = qBound<qreal>(1.0, json.value(QStringLiteral("sineAmplitude")).toDouble(20.0), 200.0);
    m_refFrame = ArrayToRect(json.value(QStringLiteral("sineFrame")).toArray(), QRectF(0.0, 0.0, 1.0, 1.0));
    Rebuild();
}
