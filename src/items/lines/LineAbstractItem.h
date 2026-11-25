#pragma once

#include "items/CanvasItem.h"
#include "items/geometry/CurveGeometry.h"
#include "items/properties/ArrowStyle.h"
#include "items/properties/StrokeAttributes.h"

#include <QColor>
#include <QJsonObject>
#include <QPainterPath>
#include <QSizeF>
#include <QVector>
#include <Qt>

// 描线图元抽象基类：几何 + 描带绘制管线；颜色 / 线宽 / 线型来自 StrokeAttributes，
// 零面积图元不继承填充属性基类。
// 五个形态（直线 / 弧线 / 正弦 / 手绘曲线 / 花括号）共同继承本类，
// 类型标识由各自的 Kind() 给出，本类不覆写 Kind()。
class LineAbstractItem : public CanvasItem, public StrokeAttributes {
  public:
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

    explicit LineAbstractItem(QGraphicsItem* parent = nullptr);

    ItemCategory Category() const override { return ItemCategory::LINE; }
    void Accept(CanvasItemVisitor& visitor) override;
    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    QPainterPath shape() const override;
    QPainterPath GeometryOutline() const override;
    QPainterPath GeometryPaintedShape() const override;

    QColor StrokeColor() const { return StrokeAttributes::StrokeColor(); }
    qreal StrokeWidth() const { return StrokeAttributes::StrokeWidth(); }
    Qt::PenStyle strokePattern() const { return StrokeAttributes::StrokeStyle(); }
    void SetStrokeColor(const QColor& color);
    void SetStrokeWidth(qreal width);
    void SetStrokePattern(Qt::PenStyle style);

    // 折线模式：每段退化成直线，方角不会被拟合成圆角（图片线稿化使用）。
    bool IsPolyline() const { return m_polyline; }

    // 用算好的场景坐标直接构造一条描线（插入与图片线稿化等程序化场景使用）。
    virtual void SetStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width,
                                 qreal tolerance = 0.0, bool polyline = false);
    virtual QVector<QPointF> StrokePoints() const;
    int SegmentCount() const { return m_segments.size(); }

    ArrowStyle StartArrow() const { return m_startArrow; }
    ArrowStyle EndArrow() const { return m_endArrow; }
    void SetArrowStyles(ArrowStyle start, ArrowStyle end);
    // 端点绑定（Excalidraw 语义）：{elementId, focus, gap}，空对象表示自由端点。
    QJsonObject StartBinding() const { return m_startBinding; }
    QJsonObject EndBinding() const { return m_endBinding; }
    void SetStartBinding(const QJsonObject& binding) { m_startBinding = binding; }
    void SetEndBinding(const QJsonObject& binding) { m_endBinding = binding; }

    // 端点绑定跟随：把某一端拖到指定场景坐标，只动这一端与控制柄，其余节点保持。
    bool MoveEndpoint(bool start, const QPointF& scenePoint);

    // 两端箭头在场景里的实际绘制范围（含描边余量）。导出包围框要把它算进去：
    // 箭头头长是线宽的 4 倍，只按半个线宽外扩会把翅膀切掉一半。
    QRectF ArrowSceneBounds() const;

    virtual GeometryState geometryState() const;
    virtual void ApplyGeometryState(const GeometryState& state);

  public:
    // 箭头行是否可用（单点几何没有首尾切线）。
    bool ArrowsEnabled() const { return ArrowsAvailable(); }

  protected:
    void PaintContent(QPainter* painter) override;
    virtual void RebuildPath();
    // 按点段几何重算内容框与原点：余量铺在四周，基坐标左上角落在描线余量处。
    virtual void NormalizeGeometry();
    // 单点笔迹（手绘落点）判定；手绘曲线覆写，其余形态不存在单点几何。
    virtual bool IsSinglePointGeometry() const { return false; }
    virtual bool ArrowsAvailable() const { return !IsSinglePointGeometry(); }


    QColor StrokeColorOrDefault(const QColor& color) const;
    QRectF SegmentBounds() const;

    void StrokeChanged() override { update(); }

    ArrowStyle m_startArrow = ArrowStyle::NONE;
    ArrowStyle m_endArrow = ArrowStyle::NONE;
    QJsonObject m_startBinding;
    QJsonObject m_endBinding;
    QVector<CurveSegment> m_segments;
    QPainterPath m_path;
    QSizeF m_baseSize{1.0, 1.0};
    // 描线余量：笔宽一半再加一点，保证描边与箭头不被内容框裁掉。
    qreal m_strokePadding = 4.0;
    bool m_polyline = false;
};
