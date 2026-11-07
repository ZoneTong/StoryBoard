#pragma once

#include "BoardItem.h"

#include <QColor>
#include <QPainterPath>
#include <QVector>

class StrokeItem : public BoardItem
{
    Q_OBJECT
public:
    explicit StrokeItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("stroke"); }
    QString displayName() const override { return QStringLiteral("笔画"); }

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;

    void beginStroke(const QPointF& scenePos, const QColor& color, qreal width);
    void appendScenePoint(const QPointF& scenePos);
    void endStroke();
    bool isEmpty() const { return m_points.isEmpty(); }

    QColor strokeColor() const { return m_color; }
    qreal strokeWidth() const { return m_penWidth; }

protected:
    void paintContent(QPainter* painter) override;

private:
    void rebuildPath();
    void normalizeToContent();
    qreal pathLength() const;

    QVector<QPointF> m_points;
    QPainterPath m_path;
    QColor m_color = QColor(30, 41, 59);
    qreal m_penWidth = 4.0;
    QSizeF m_baseSize { 1.0, 1.0 };
    bool m_dot = false;
    bool m_finished = false;
    qreal m_pad = 4.0;
};
