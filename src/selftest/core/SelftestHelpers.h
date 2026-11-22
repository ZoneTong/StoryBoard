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
class CurveItem;

namespace selftest {

QString GeometryText(const QPointF& pos, const QSizeF& size);
QPixmap MakeShotImage(int width, int height, const QString& label, const QColor& start, const QColor& end);
QPixmap MakeTextImage(const QString& text, const QColor& color, const QFont& font, int width, int height);
void AddStroke(CanvasScene& scene, const QVector<QPointF>& points, const QColor& color, qreal width, int layerId = -1);
void AddLine(CanvasScene& scene, const QPointF& from, const QPointF& to, const QColor& color, qreal width,
             int layerId = -1);
CurveItem* MakeLineItem(const QPointF& from, const QPointF& to, const QColor& color);
void SendMouseEvent(QWidget* target, QEvent::Type type, const QPoint& position, Qt::MouseButton button,
                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
QString DemoCanvasPath();

} // namespace selftest
