#pragma once

#include "ArrowStyle.h"
#include "CanvasItem.h"
#include "CurveGeometry.h"

#include <QColor>
#include <QPainterPath>
#include <QSizeF>
#include <QVector>
#include <Qt>

// 描线图元抽象基类：描线属性 + 点段几何 + 描带 / 两端的箭头绘制管线。
// 五个形态（直线 / 弧线 / 正弦 / 手绘曲线 / 花括号）共同继承本类，
// 类型标识由各自的 kind() 给出，本类不覆写 kind()。
class LineItemAbstract : public CanvasItem {
  public:
    // 参数快照：弧线是起止角，正弦是周期数与振幅；其余形态 valid 为 false。
    struct ParamState {
        qreal first = 0.0;
        qreal second = 0.0;
        bool valid = false;
    };

    // 一次几何变化（拟合 / 简化 / 参数重建）的完整快照，撤销与重做都靠它。
    struct GeometryState {
        QVector<CurveSegment> segments;
        QPointF pos;
        QSizeF contentSize;
        QSizeF baseSize;
        bool hasDot = false;
        QPointF dotCenter;

        bool operator==(const GeometryState& other) const;
        bool operator!=(const GeometryState& other) const { return !(*this == other); }
    };

    explicit LineItemAbstract(QGraphicsItem* parent = nullptr);

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;
    QPainterPath shape() const override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;

    QColor strokeColor() const { return m_strokeColor; }
    qreal strokeWidth() const { return m_strokeWidth; }
    Qt::PenStyle strokePattern() const { return m_strokePattern; }
    void setStrokeColor(const QColor& color);
    void setStrokeWidth(qreal width);
    void setStrokePattern(Qt::PenStyle style);

    // 折线模式：每段退化成直线，方角不会被拟合成圆角（图片线稿化使用）。
    bool isPolyline() const { return m_polyline; }

    // 用算好的场景坐标直接构造一条描线（插入与图片线稿化等程序化场景使用）。
    virtual void setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width,
                                 qreal tolerance = 0.0, bool polyline = false);
    virtual QVector<QPointF> strokePoints() const;
    int segmentCount() const { return m_segments.size(); }

    ArrowStyle startArrow() const { return m_startArrow; }
    ArrowStyle endArrow() const { return m_endArrow; }
    void setArrowStyles(ArrowStyle start, ArrowStyle end);

    virtual GeometryState geometryState() const;
    virtual void applyGeometryState(const GeometryState& state);

    // 简化曲线：能力位。默认 false，只有手绘笔迹 / 图片线稿覆写真实实现。
    virtual bool simplifyCurve(qreal tolerance);
    // 是否具备简化能力（命令入口据此决定可用性，不按叶子类强转）。
    virtual bool supportsSimplify() const { return false; }
    // 参数化线条的能力查询：弧线可改起止角，正弦可改周期与振幅。
    virtual bool supportsArcAngles() const { return false; }
    virtual bool supportsSineWave() const { return false; }

    virtual ParamState paramState() const { return ParamState(); }
    virtual void applyParamState(const ParamState& state) { Q_UNUSED(state) }

  public:
    // 箭头行是否可用（单点几何没有首尾切线）。
    bool arrowsEnabled() const { return arrowsAvailable(); }

  protected:
    void paintContent(QPainter* painter) override;
    virtual void rebuildPath();
    // 按点段几何重算内容框与原点：余量铺在四周，基坐标左上角落在描线余量处。
    virtual void normalizeGeometry();
    // 单点笔迹（手绘落点）判定；手绘曲线覆写，其余形态不存在单点几何。
    virtual bool isSinglePointGeometry() const { return false; }
    virtual bool arrowsAvailable() const { return !isSinglePointGeometry(); }


    QColor strokeColorOrDefault(const QColor& color) const;
    QRectF segmentBounds() const;

    QColor m_strokeColor = QColor(30, 41, 59);
    qreal m_strokeWidth = 4.0;
    Qt::PenStyle m_strokePattern = Qt::SolidLine;
    ArrowStyle m_startArrow = ArrowStyle::NONE;
    ArrowStyle m_endArrow = ArrowStyle::NONE;
    QVector<CurveSegment> m_segments;
    QPainterPath m_path;
    QSizeF m_baseSize{1.0, 1.0};
    // 描线余量：笔宽一半再加一点，保证描边与箭头不被内容框裁掉。
    qreal m_strokePadding = 4.0;
    bool m_polyline = false;
};
