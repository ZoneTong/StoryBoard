#pragma once

#include "I18n.h"
#include "LineItemAbstract.h"

// 直线图元：几何恒为两点线性段，零面积、无填充。
// 直线箭头复用同一个类，只是两端箭头样式不同。
class StraightLineItem : public LineItemAbstract {
    Q_OBJECT
  public:
    explicit StraightLineItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("straightline"); }
    QString displayName() const override;
    // 自动命名与属性窗标题固定用「直线」，箭头状态下也不改成「直线箭头」。
    QString typeDisplayName() const override { return uiText(QStringLiteral("Straight Line")); }
    // 贴近细线的内容框抓不住，选框至少 12×12（只影响选框与手柄，不改几何与命中）。
    QRectF frameRect() const override;

    // 端点用场景坐标给出；插入与「手绘近直线转生」共用这一入口。
    void setEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color, qreal width);

    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;
};
