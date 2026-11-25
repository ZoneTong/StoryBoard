#ifndef EXCALIDRAWFORMATINTERNAL_H
#define EXCALIDRAWFORMATINTERNAL_H

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

// Excalidraw 纯转换的内部辅助：样式、颜色与元素字段的读写。
// 只在本目录的三个实现文件之间共享，不对外暴露。
namespace ExcalidrawFormatDetail {

qreal NumberOr(const QJsonObject& object, const QString& key, qreal fallback);
QColor ColorOr(const QJsonObject& object, const QString& key, const QColor& fallback);
int PenStyleFromExcalidraw(const QJsonValue& value);
QString ExcalidrawStrokeStyle(int penStyle);
QString FillStyleFromExcalidraw(const QString& style);
QString ExcalidrawFillStyle(const QString& style);
QString TextFontFamilyFromExcalidraw(const QJsonValue& value);
int ExcalidrawFontFamily(const QString& family);
QString HeadStyle(const QJsonValue& value);
bool IsLineKind(const QString& kind);
QJsonObject ItemBase(const QString& kind, const QRectF& bounds, int z, int layer);
QString HeadValue(const QJsonObject& item, const QString& key);
QJsonObject StyleCustomData(const QJsonObject& item);
QJsonObject HeadCustomData(const QJsonObject& item);

QJsonArray PolylineSegments(const QList<QPointF>& points, const QPointF& origin = QPointF());
QList<QPointF> AbsolutePoints(const QJsonObject& element, QStringList* warnings);
QRectF BoundsOf(const QList<QPointF>& points);
QString CustomText(const QJsonObject& element, const QString& key);
qreal CustomNumber(const QJsonObject& element, const QString& key, qreal fallback);
QString ImportHeadStyle(const QJsonObject& element, const QString& key);
QJsonObject ElementToItem(const QJsonObject& element, int z, int layer, QStringList* warnings,
                          const QJsonObject& files, bool keepImages);
QJsonObject StoryBoardShape(const QJsonObject& item);
QJsonObject BaseElement(const QJsonObject& item, const QString& id);
QJsonArray RelativePoints(const QJsonArray& segments, qreal originX, qreal originY);

} // namespace ExcalidrawFormatDetail

#endif // EXCALIDRAWFOMRATINTERNAL_H
