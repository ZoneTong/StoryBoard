#pragma once

#include "app/I18n.h"
#include "items/lines/LineAbstractItem.h"

// 直线图元：几何恒为两点线性段，零面积、无填充。
// 直线箭头复用同一个类，只是两端箭头样式不同。
class StraightLineItem : public LineAbstractItem {
    Q_OBJECT
  public:
    explicit StraightLineItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("straightline"); }
    QString DisplayName() const override;
    // 自动命名与属性窗标题固定用「直线」，箭头状态下也不改成「直线箭头」。
    QString TypeDisplayName() const override { return UiText(QStringLiteral("Straight Line")); }
    // 贴近细线的内容框抓不住，选框至少 12×12（只影响选框与手柄，不改几何与命中）。
    QRectF FrameRect() const override;
    // 直线只由两个端点定位：选中时不画外接框、缩放手柄与旋转手柄，只留端点方块。
    bool HasSelectionFrame() const override { return false; }
    // 编辑面板的角度就是直线走向：斜了自动跟随，写入时按绝对角度旋转。
    qreal DisplayRotationAngle() const override;
    void SetDisplayRotationAngle(qreal angle) override;
    // 两端端点句柄：拖动时直接改端点，并按 12px 口径重新绑定 / 解除绑定。
    HandleSpecList CustomHandles() const override;
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;

    // 端点用场景坐标给出；插入与「手绘近直线转生」共用这一入口。
    void SetEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color, qreal width);

    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
};
