#pragma once

#include "I18n.h"
#include "LineItemAbstract.h"

#include <QColor>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

// 笔迹来源：只描述曲线怎么产生（手绘 / 图片线稿提取），不参与类型判断。
enum class CurveSource { HAND_DRAWN = 0, LINE_ART };

// 手绘曲线图元：内部几何是分段三次贝塞尔，绘制期的采样点只在收笔拟合时短暂存在，
// 不写入画布文件。直线 / 弧线 / 正弦 / 花括号已各自成类，本类只剩笔迹与线稿。
class CurveItem : public LineItemAbstract {
    Q_OBJECT
  public:
    explicit CurveItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("freehand"); }
    QString displayName() const override;

    void beginStroke(const QPointF& scenePos, const QColor& color, qreal width);
    void appendScenePoint(const QPointF& scenePos);
    // 用算好的场景坐标直接构造一条曲线（图片线稿化等程序化场景使用）。
    void setStrokePoints(const QVector<QPointF>& scenePoints, const QColor& color, qreal width, qreal tolerance = 0.0,
                         bool polyline = false) override;
    // tolerance 是绘制期拟合参数：0 表示按采样点直接连成曲线段，>0 时先抽点再拟合。
    void endStroke(qreal tolerance = 0.0);
    bool isEmpty() const;

    CurveSource source() const { return m_strokeSource; }
    void setSource(CurveSource source);

    GeometryState geometryState() const override;
    void applyGeometryState(const GeometryState& state) override;
    // 简化曲线：把当前路径拍平后按容差抽点并重新拟合；返回是否改变了几何。
    bool simplifyCurve(qreal tolerance) override;
    bool supportsSimplify() const override { return true; }

    QJsonObject toJson() const override;
    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;
    void fromJson(const QJsonObject& json) override;

  protected:
    void rebuildPath() override;
    void normalizeGeometry() override;
    bool isSinglePointGeometry() const override;

  private:
    // 仅绘制期存在：收笔前保存原始采样点，用于预览与拟合。
    QVector<QPointF> m_rawPoints;
    bool m_hasDot = false;
    bool m_drawing = false;
    bool m_finished = false;
    CurveSource m_strokeSource = CurveSource::HAND_DRAWN;
};
