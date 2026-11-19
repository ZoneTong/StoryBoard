#include "ArcItem.h"

#include <QJsonArray>
#include <QVector>

namespace {

constexpr qreal kMinimumFrameSide = 12.0;
constexpr int kArcSamples = 64;

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

ArcItem::ArcItem(QGraphicsItem* parent) : CurveItem(parent)
{
    setCurveKind(CurveKind::Arc);
}

QString ArcItem::displayName() const
{
    return uiText(QStringLiteral("Arc"));
}

QRectF ArcItem::frameRect() const
{
    return paddedToMinimum(CanvasItem::frameRect());
}

void ArcItem::setAngles(qreal startDeg, qreal sweepDeg)
{
    const qreal sweep = EllipseShape::clampedSweep(sweepDeg);
    if (qFuzzyCompare(startDeg + 1.0, m_startAngle + 1.0) && qFuzzyCompare(sweep + 1.0, m_sweepAngle + 1.0)) {
        return;
    }
    m_startAngle = startDeg;
    m_sweepAngle = sweep;
    rebuild();
}

void ArcItem::setArcGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                             const QColor& color, qreal width)
{
    m_startAngle = startDeg;
    m_sweepAngle = EllipseShape::clampedSweep(sweepDeg);

    const QRectF frameScene(topLeftScene, size);
    setFrame(frameScene);
    setStrokePoints(sample(m_startAngle, m_sweepAngle, kArcSamples), color, width, 0.0, false);
    setCurveKind(CurveKind::Arc);

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

void ArcItem::rebuild()
{
    const QSizeF box = contentSize();
    if (box.width() <= 0.0 || box.height() <= 0.0) {
        return;
    }
    const QRectF local(m_relFrame.x() * box.width(), m_relFrame.y() * box.height(),
                       m_relFrame.width() * box.width(), m_relFrame.height() * box.height());
    const QRectF frameScene = local.translated(pos());
    setFrame(frameScene);
    setStrokePoints(sample(m_startAngle, m_sweepAngle, kArcSamples), strokeColor(), strokeWidth(), 0.0, false);

    const QPointF origin = pos();
    const QSizeF now = contentSize();
    if (now.width() > 0.0 && now.height() > 0.0) {
        m_relFrame = QRectF((frameScene.left() - origin.x()) / now.width(),
                            (frameScene.top() - origin.y()) / now.height(), frameScene.width() / now.width(),
                            frameScene.height() / now.height());
    }
    update();
}

QJsonObject ArcItem::toJson() const
{
    QJsonObject json = CurveItem::toJson();
    json.insert(QStringLiteral("startAngle"), m_startAngle);
    json.insert(QStringLiteral("sweepAngle"), m_sweepAngle);
    json.insert(QStringLiteral("arcFrame"), toArray(m_relFrame));
    return json;
}

void ArcItem::fromJson(const QJsonObject& json)
{
    CurveItem::fromJson(json);
    m_startAngle = json.value(QStringLiteral("startAngle")).toDouble(30.0);
    m_sweepAngle = EllipseShape::clampedSweep(json.value(QStringLiteral("sweepAngle")).toDouble(120.0));
    m_relFrame = fromArray(json.value(QStringLiteral("arcFrame")).toArray(), QRectF(0.0, 0.0, 1.0, 1.0));
    setCurveKind(CurveKind::Arc);
    rebuild();
}
