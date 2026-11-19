#include "SineItem.h"

#include <QJsonArray>
#include <QtMath>

namespace {

constexpr qreal kMinimumFrameSide = 12.0;
constexpr int kStepsPerCycle = 64;

QRectF paddedToMinimum(QRectF frame)
{
    if (frame.width() < kMinimumFrameSide) {
        frame.moveLeft(frame.center().x() - kMinimumFrameSide / 2.0);
        frame.setWidth(kMinimumFrameSide);
    }
    if (frame.height() < kMinimumFrameSide) {
        frame.moveTop(frame.center().y() - kMinimumFrameSide / 2.0);
        frame.setHeight(kMinimumFrameSide);
    }
    return frame;
}

QJsonArray toArray(const QRectF& rect)
{
    QJsonArray array;
    array.append(rect.x());
    array.append(rect.y());
    array.append(rect.width());
    array.append(rect.height());
    return array;
}

QRectF fromArray(const QJsonArray& array, const QRectF& fallback)
{
    if (array.size() < 4) {
        return fallback;
    }
    return QRectF(array.at(0).toDouble(), array.at(1).toDouble(), array.at(2).toDouble(), array.at(3).toDouble());
}

} // namespace

SineItem::SineItem(QGraphicsItem* parent) : CurveItem(parent)
{
    setCurveKind(CurveKind::Sine);
}

QString SineItem::displayName() const
{
    return uiText(QStringLiteral("Sine wave"));
}

QRectF SineItem::frameRect() const
{
    return paddedToMinimum(CanvasItem::frameRect());
}

void SineItem::setParams(const Params& params)
{
    const int cycles = qBound(1, params.cycles, 12);
    const qreal amplitude = qBound<qreal>(1.0, params.amplitude, 200.0);
    if (cycles == m_params.cycles && qFuzzyCompare(amplitude, m_params.amplitude)) {
        return;
    }
    m_params.cycles = cycles;
    m_params.amplitude = amplitude;
    rebuild();
}

QVector<QPointF> SineItem::wavePoints(const QRectF& frameScene) const
{
    QVector<QPointF> points;
    if (frameScene.width() <= 0.0 || frameScene.height() <= 0.0) {
        return points;
    }

    const int steps = kStepsPerCycle * m_params.cycles;
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

void SineItem::setWaveGeometry(const QPointF& topLeftScene, const QSizeF& size, const QColor& color, qreal width)
{
    const QRectF frameScene(topLeftScene, size);
    setStrokePoints(wavePoints(frameScene), color, width, 0.0, false);
    setCurveKind(CurveKind::Sine);

    const QPointF origin = pos();
    const QSizeF box = contentSize();
    if (box.width() > 0.0 && box.height() > 0.0) {
        m_relFrame = QRectF((frameScene.left() - origin.x()) / box.width(),
                            (frameScene.top() - origin.y()) / box.height(),
                            frameScene.width() / box.width(), frameScene.height() / box.height());
    } else {
        m_relFrame = QRectF(0.0, 0.0, 1.0, 1.0);
    }
}

void SineItem::contentResized()
{
    if (m_rebuilding) {
        return;
    }
    // 内容框高度 = 波高 + 笔宽余量；拖高拖矮等价于改振幅，参数跟着几何走，面板回显也随之更新。
    const qreal boxHeight = contentSize().height();
    if (boxHeight <= 0.0) {
        return;
    }
    const qreal padding = qMax<qreal>(6.0, strokeWidth() + 4.0);
    const qreal amplitude = qBound<qreal>(1.0, qMax<qreal>(1.0, boxHeight - padding) / 2.0, 200.0);
    if (qAbs(amplitude - m_params.amplitude) < 0.5) {
        return;
    }
    m_params.amplitude = amplitude;
    rebuild();
}

void SineItem::rebuild()
{
    const QSizeF box = contentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_relFrame.x() * box.width(), m_relFrame.y() * box.height(),
                       m_relFrame.width() * box.width(), m_relFrame.height() * box.height());
    const QRectF frameScene = local.translated(pos());
    m_rebuilding = true;
    setStrokePoints(wavePoints(frameScene), strokeColor(), strokeWidth(), 0.0, false);
    m_rebuilding = false;

    const QPointF origin = pos();
    const QSizeF now = contentSize();
    if (now.width() > 0.0 && now.height() > 0.0) {
        m_relFrame = QRectF((frameScene.left() - origin.x()) / now.width(),
                            (frameScene.top() - origin.y()) / now.height(), frameScene.width() / now.width(),
                            frameScene.height() / now.height());
    }
    update();
}

QJsonObject SineItem::toJson() const
{
    QJsonObject json = CurveItem::toJson();
    json.insert(QStringLiteral("sineCycles"), m_params.cycles);
    json.insert(QStringLiteral("sineAmplitude"), m_params.amplitude);
    json.insert(QStringLiteral("sineFrame"), toArray(m_relFrame));
    return json;
}

void SineItem::fromJson(const QJsonObject& json)
{
    CurveItem::fromJson(json);
    m_params.cycles = qBound(1, json.value(QStringLiteral("sineCycles")).toInt(1), 12);
    m_params.amplitude = qBound<qreal>(1.0, json.value(QStringLiteral("sineAmplitude")).toDouble(20.0), 200.0);
    m_relFrame = fromArray(json.value(QStringLiteral("sineFrame")).toArray(), QRectF(0.0, 0.0, 1.0, 1.0));
    setCurveKind(CurveKind::Sine);
    rebuild();
}
