#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// 平行四边形族抽象基类：承载内角 A、角度浮层与状态 / JSON 增量。
// 只放族公共行为，不暴露矩形圆角、菱形高度反算等叶子特有属性和接口。
// 内角归属：普通平行四边形以角度为落盘真相；矩形锁 90°；菱形由宽高派生。
// 内角倾斜旋钮只属于普通平行四边形（句柄见 GeneralParallelogramItem::CustomHandles）。
class ParallelogramAbstractItem : public ShapeAbstractItem {
  public:
    // 内角 A（度）。菱形覆写为派生量（A = 2·atan(h/w)），其余成员读写字段。
    virtual qreal ParallelogramAngle() const { return m_parallelogramAngle; }
    virtual void SetParallelogramAngle(qreal angle);
    // 普通平行四边形的左下顶点（内容区局部坐标）：角度浮层与旋钮拖拽的公共锚点。
    QPointF BottomLeftVertex() const;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    // 内角 A 属于本族：状态快照按 ItemProperty 写进 propertyExtras，不再占公共字段。
    void WriteState(ShapeItemState& state) const override;
    // §3.2：菱形拖外接框 / 边时点亮派生内角读数（尺寸是真相，角度随宽高连续变化）。
    void FrameResizeDragged() override;

  protected:
    void ApplyState(const ShapeItemState& state) override;
    // 角度浮层锚点：默认贴左下顶点（§3.2 不再贴旋钮），菱形覆写为左顶点。
    virtual QPointF AngleReadoutAnchor() const;
    // 读数框：左下内角处，随顶点移动（锐角 / 钝角都贴住真正的角）。
    QRectF AngleReadoutRect() const;
    // 状态 / JSON 回放用：只写字段，不带叶子副作用（改尺寸、保锚点由叶子负责）。
    void ApplyParallelogramAngleRaw(qreal angle) { m_parallelogramAngle = angle; }
    void DrawSelectionOverlay(QPainter* painter) override;
    QList<QRectF> AngleReadoutRects() const override;
    void WriteJsonExtra(QJsonObject& json) const override;
    void ReadJsonExtra(const QJsonObject& json) override;

  private:
    qreal m_parallelogramAngle = 60.0;
};
