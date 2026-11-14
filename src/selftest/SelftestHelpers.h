#pragma once

#include <QColor>
#include <QEvent>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

class CanvasScene;
class QFont;
class QPoint;
class QPixmap;
class QWidget;
class StrokeItem;

namespace selftest {

QString geometryText(const QPointF& pos, const QSizeF& size);
QPixmap makeShotImage(int width, int height, const QString& label, const QColor& start, const QColor& end);
QPixmap makeTextImage(const QString& text, const QColor& color, const QFont& font, int width, int height);
void addStroke(CanvasScene& scene, const QVector<QPointF>& points, const QColor& color, qreal width, int layerId = -1);
void addLine(CanvasScene& scene, const QPointF& from, const QPointF& to, const QColor& color, qreal width,
             int layerId = -1);
StrokeItem* makeLineItem(const QPointF& from, const QPointF& to, const QColor& color);
void sendMouseEvent(QWidget* target, QEvent::Type type, const QPoint& position, Qt::MouseButton button,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
QString demoCanvasPath();

} // namespace selftest
