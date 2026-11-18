#pragma once

#include "ArrowStyle.h"
#include "CanvasItem.h"
#include "CurveGeometry.h"
#include "I18n.h"

#include <QColor>
#include <QPainterPath>
#include <QVector>
#include <Qt>

// 曲线图元：内部几何是分段三次贝塞尔，绘制期的采样点只在收笔拟合时短暂存在，
// 不写入画布文件。属性与直线对齐：描线颜色 / 宽度 / 样式、透明度、位置尺寸、
// 旋转、名称、图层与层内高度；没有填充与平滑度属性。
// 需求 13：括号复用曲线类，需要知道曲线来源，才能对括号隐藏箭头行、跳过简化。
enum class CurveSource { HandDrawn = 0, LineArt, Brace };

class CurveItem : public CanvasItem {
    Q_OBJECT
  public:
    // 拟合会同时改变贝塞尔段、内容尺寸和位置，撤销/重做需要完整快照。
    struct GeometryState {
        QVector<CurveSegment> segments;
        QPointF pos;
        QSizeF contentSize;
        QSizeF baseSize;
        bool dot = false;
        QPointF dotCenter;

        bool operator==(const GeometryState& other) const;
        bool operator!=(const GeometryState& other) const { return !(*this == other); }
    };

    explicit CurveItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("curve"); }
    QString displayName() const override;

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;

    void beginStroke(const QPointF& scenePos, const QColor& color, qreal width);
    void appendScenePoint(const QPointF& scenePos);
    // 用算好的场景坐标直接构造一条曲线（图片线稿化等程序化场景使用）。
    void setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width, qreal tolerance = 0.0,
                         bool linearPath = false);
    // tolerance 是绘制期拟合参数：0 表示按采样点直接连成曲线段，>0 时先抽点再拟合。
    void endStroke(qreal tolerance = 0.0);
    bool isEmpty() const;

    QColor strokeColor() const { return m_color; }
    qreal strokeWidth() const { return m_penWidth; }
    Qt::PenStyle strokeStyle() const { return m_penStyle; }
    bool isLinearPath() const { return m_linearPath; }
    void setStyle(const QColor& color, qreal width);
    void setStrokeStyle(Qt::PenStyle style);
    // 需求 3.12：曲线也支持两端箭头样式，几何与描线宽度不变。
    ArrowStyle startArrow() const { return m_startArrow; }
    ArrowStyle endArrow() const { return m_endArrow; }
    void setArrowStyles(ArrowStyle start, ArrowStyle end);

    CurveSource source() const { return m_source; }
    void setSource(CurveSource source);

    int segmentCount() const { return m_segments.size(); }
    // 曲线段的端点序列，供只读检查与撤销比较；不再暴露原始采样点。
    int strokePointCount() const { return CurveGeometry::knots(m_segments).size(); }
    QVector<QPointF> strokePoints() const { return CurveGeometry::knots(m_segments); }
    GeometryState geometryState() const;
    void applyGeometryState(const GeometryState& state);
    // 简化曲线：把当前路径拍平后按容差抽点并重新拟合；返回是否改变了几何。
    bool simplifyCurve(qreal tolerance);
    // 兼容旧调用名，行为等同 simplifyCurve。
    bool smoothStroke(qreal tolerance) { return simplifyCurve(tolerance); }

  protected:
    void paintContent(QPainter* painter) override;

  private:
    void rebuildPath();
    void normalizeToContent();
    QRectF geometryBounds() const;

    QVector<CurveSegment> m_segments;
    // 仅绘制期存在：收笔前保存原始采样点，用于预览与拟合。
    QVector<QPointF> m_rawPoints;
    QPainterPath m_path;
    QColor m_color = QColor(30, 41, 59);
    qreal m_penWidth = 4.0;
    Qt::PenStyle m_penStyle = Qt::SolidLine;
    QSizeF m_baseSize{1.0, 1.0};
    bool m_dot = false;
    bool m_drawing = false;
    bool m_finished = false;
    // 折线曲线：每段退化成直线，方角不会被拟合成圆角（图片线稿化使用）。
    bool m_linearPath = false;
    qreal m_pad = 4.0;
    ArrowStyle m_startArrow = ArrowStyle::None;
    ArrowStyle m_endArrow = ArrowStyle::None;
    CurveSource m_source = CurveSource::HandDrawn;
};
