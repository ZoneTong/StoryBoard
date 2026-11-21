#include "selftest/SelftestHelpers.h"

#include "CanvasScene.h"
#include "CurveItem.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

namespace selftest {

QString GeometryText(const QPointF& pos, const QSizeF& size)
{
    return QStringLiteral("pos=(%1,%2) size=(%3,%4)")
        .arg(pos.x(), 0, 'f', 1)
        .arg(pos.y(), 0, 'f', 1)
        .arg(size.width(), 0, 'f', 1)
        .arg(size.height(), 0, 'f', 1);
}

QPixmap MakeShotImage(int width, int height, const QString& label, const QColor& start, const QColor& end)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, width, height), 18, 18);
    painter.setClipPath(clip);

    QLinearGradient gradient(0, 0, 0, height);
    gradient.setColorAt(0.0, start);
    gradient.setColorAt(1.0, end);
    painter.fillRect(QRectF(0, 0, width, height), gradient);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 70));
    painter.drawEllipse(QPointF(width * 0.76, height * 0.28), height * 0.16, height * 0.16);
    painter.setBrush(QColor(24, 40, 72, 55));
    QPainterPath hills;
    hills.moveTo(0, height);
    hills.lineTo(width * 0.28, height * 0.52);
    hills.lineTo(width * 0.48, height * 0.72);
    hills.lineTo(width * 0.68, height * 0.45);
    hills.lineTo(width, height * 0.78);
    hills.lineTo(width, height);
    hills.closeSubpath();
    painter.drawPath(hills);

    painter.setPen(QColor(255, 255, 255, 235));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSizeF(height * 0.17);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(18, 12, width - 36, height * 0.4), Qt::AlignLeft | Qt::AlignTop, label);
    painter.end();
    return pixmap;
}

QPixmap MakeTextImage(const QString& text, const QColor& color, const QFont& font, int width, int height)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(color);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, width, height), Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.end();
    return pixmap;
}

void AddStroke(CanvasScene& scene, const QVector<QPointF>& points, const QColor& color, qreal width, int layerId)
{
    auto* stroke = new CurveItem();
    stroke->SetLayerId(layerId);
    stroke->BeginStroke(points.constFirst(), color, width);
    for (int i = 1; i < points.size(); ++i) {
        stroke->AppendScenePoint(points.at(i));
    }
    stroke->EndStroke();
    scene.AttachItem(stroke);
}

void AddLine(CanvasScene& scene, const QPointF& from, const QPointF& to, const QColor& color, qreal width, int layerId)
{
    AddStroke(scene, {from, to}, color, width, layerId);
}

CurveItem* MakeLineItem(const QPointF& from, const QPointF& to, const QColor& color)
{
    auto* item = new CurveItem();
    item->BeginStroke(from, color, 4.0);
    item->AppendScenePoint(to);
    item->EndStroke();
    return item;
}

void SendMouseEvent(QWidget* target, QEvent::Type type, const QPoint& position, Qt::MouseButton button,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(type, QPointF(position), QPointF(target->mapToGlobal(position)), button, buttons, modifiers);
    QApplication::sendEvent(target, &event);
}

QString DemoCanvasPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString adjacent = QDir(appDir).filePath(QStringLiteral("examples/demo-canvas.sbcanvas"));
    if (QFileInfo::exists(adjacent)) {
        return adjacent;
    }
    return QDir(appDir).filePath(QStringLiteral("../../examples/demo-canvas.sbcanvas"));
}

} // namespace selftest
