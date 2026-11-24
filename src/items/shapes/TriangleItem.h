#pragma once

#include "items/shapes/ShapeAbstractItem.h"

// 三角形：底边两侧的底角是真相，高度由底边与角度派生；顶点句柄、缩放反算 B、
// 角度读数等三角形专属行为全部收敛在本类（§3.1 / G3）。底边短刻度线按 §D9 删除。
class TriangleItem : public ShapeAbstractItem {
  public:
    TriangleItem();
    QSizeF DefaultContentSize() const override;
    ShapeType shapeType() const override { return ShapeType::TRIANGLE; }
    ItemPropertySet Properties() const override;
    // 底角 A 处的内角旋钮：贴着左下顶点绕它拖，只改底角 A（§3.1）。
    HandleSpecList CustomHandles() const override;

    qreal BottomAngle1() const override { return m_bottomAngle1; }
    qreal BottomAngle2() const override { return m_bottomAngle2; }
    void SetTriangleAngles(qreal angle1, qreal angle2) override;
    void SetTriangleAnglesFixedHeight(qreal angle1, qreal angle2, qreal height) override;
    void ApplyState(const ShapeItemState& state) override;
    void FromJson(const QJsonObject& json) override;
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;

  protected:
    QPainterPath ShapePath(const QRectF& rect) const override;
    void ApplyStateContentSize(const QSizeF& requested) override;
    void ContentResized() override;
    void FrameResizeDragged() override;
    void DrawSelectionOverlay(QPainter* painter) override;
    QList<QRectF> AngleReadoutRects() const override;
    void WriteJsonExtra(QJsonObject& json) const override;
    void ReadJsonExtra(const QJsonObject& json) override;

  private:
    // 两底角是真相；默认等角 60°，高度由 AspectRatio 派生。
    qreal m_bottomAngle1 = 60.0;
    qreal m_bottomAngle2 = 60.0;
    // 角度 / 状态回放期间的尺寸同步屏蔽：避免 SetContentSize 触发的
    // ContentResized 反算 B 覆盖刚写入的角度。
    bool m_syncingTriangle = false;
    // 状态回放时记录改动前的角度，用于“同一尺寸、只改角度”时保持底边长度。
    qreal m_stateOldAngle1 = 60.0;
    qreal m_stateOldAngle2 = 60.0;
    // 拖 A 角旋钮期间固定的底边锚点（场景坐标）、AB 边上的垂直高度与起手角度 / 指针朝向。
    QPointF m_apexDragBaseA;
    QPointF m_apexDragBaseB;
    qreal m_apexDragHeight = 1.0;
    qreal m_apexDragStartAngle = 0.0;
    qreal m_apexDragStartPointer = 0.0;
};
