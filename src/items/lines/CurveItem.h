#pragma once

#include "app/I18n.h"
#include "items/lines/LineAbstractItem.h"

#include <QColor>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

// 笔迹来源：只描述曲线怎么产生（手绘 / 图片线稿提取），不参与类型判断。
enum class CurveSource { HAND_DRAWN = 0, LINE_ART };

// 手绘曲线图元：内部几何是分段三次贝塞尔，绘制期的采样点只在收笔拟合时短暂存在，
// 不写入画布文件。直线 / 弧线 / 正弦 / 花括号已各自成类，本类只剩笔迹与线稿。
class CurveItem : public LineAbstractItem {
    Q_OBJECT
  public:
    explicit CurveItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("freehand"); }
    QString DisplayName() const override;

    void BeginStroke(const QPointF& scenePos, const QColor& color, qreal width);
    void AppendScenePoint(const QPointF& scenePos);
    // 用算好的场景坐标直接构造一条曲线（图片线稿化等程序化场景使用）。
    void SetStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width, qreal tolerance = 0.0,
                         bool polyline = false) override;
    // tolerance 是绘制期拟合参数：0 表示按采样点直接连成曲线段，>0 时先抽点再拟合。
    void EndStroke(qreal tolerance = 0.0);
    bool IsEmpty() const;

    CurveSource StrokeSource() const { return m_strokeSource; }
    void SetStrokeSource(CurveSource source);

    GeometryState geometryState() const override;
    void ApplyGeometryState(const GeometryState& state) override;
    // 简化曲线：把当前路径拍平后按容差抽点并重新拟合；返回是否改变了几何。
    // 这是曲线独有的操作，不放在描线基类上，场景通过 Visitor 调用。
    bool SimplifyCurve(qreal tolerance);
    // 是否值得简化：单点笔迹与不足三个节点的几何直接跳过。
    bool CanSimplify() const;
    void Accept(CanvasItemVisitor& visitor) override;

    QJsonObject ToJson() const override;
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    void FromJson(const QJsonObject& json) override;

  protected:
    void RebuildPath() override;
    void NormalizeGeometry() override;
    bool IsSinglePointGeometry() const override;

  private:
    // 仅绘制期存在：收笔前保存原始采样点，用于预览与拟合。
    QVector<QPointF> m_rawPoints;
    bool m_hasDot = false;
    bool m_drawing = false;
    bool m_finished = false;
    CurveSource m_strokeSource = CurveSource::HAND_DRAWN;
};
