#pragma once

#include "CanvasItem.h"
#include "I18n.h"

#include <QColor>
#include <QPainterPath>
#include <QVector>

class StrokeItem : public CanvasItem {
    Q_OBJECT
  public:
    // 平滑会同时改变局部采样点、内容尺寸和位置，撤销/重做需要完整快照。
    struct GeometryState {
        QVector<QPointF> points;
        QPointF pos;
        QSizeF contentSize;
        QSizeF baseSize;
        bool dot = false;

        bool operator==(const GeometryState& other) const;
        bool operator!=(const GeometryState& other) const { return !(*this == other); }
    };

    explicit StrokeItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("stroke"); }
    QString displayName() const override { return tr("Stroke"); }

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;

    void beginStroke(const QPointF& scenePos, const QColor& color, qreal width);
    void appendScenePoint(const QPointF& scenePos);
    // 用算好的场景坐标直接构造一条笔画（图片线稿化等程序化场景使用）。
    // 不走 appendScenePoint 的最小间距过滤，密集的轮廓点不会被丢掉。
    void setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width,
                         qreal smoothingTolerance = 0.0, bool linearPath = false);
    // smoothingTolerance > 0 时先判断是否接近直线，直线直接收敛成两点，
    // 曲线则按容差简化并做角切平滑；0 表示不做自动平滑。
    void endStroke(qreal smoothingTolerance = 0.0);
    bool isEmpty() const { return m_points.isEmpty(); }

    QColor strokeColor() const { return m_color; }
    qreal strokeWidth() const { return m_penWidth; }
    bool isLinearPath() const { return m_linearPath; }
    void setStyle(const QColor& color, qreal width);

    int strokePointCount() const { return m_points.size(); }
    QVector<QPointF> strokePoints() const { return m_points; }
    GeometryState geometryState() const;
    void applyGeometryState(const GeometryState& state);
    // 手动平滑一次；返回是否真的改变了采样点。
    bool smoothStroke(qreal tolerance);

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
    QSizeF m_baseSize{1.0, 1.0};
    bool m_dot = false;
    bool m_finished = false;
    // 折线笔画：路径按直线段连接，方角不会被平滑成圆角（图片线稿化使用）。
    bool m_linearPath = false;
    qreal m_pad = 4.0;
};
