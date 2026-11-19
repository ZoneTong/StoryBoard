#pragma once

#include "CurveItem.h"

// 直线图元：曲线族的受限子型——几何恒为两点线性段，零面积、无填充。
// 直线箭头复用同一个类，只是两端箭头样式不同。
class LineItem : public CurveItem {
    Q_OBJECT
  public:
    explicit LineItem(QGraphicsItem* parent = nullptr);

    QString displayName() const override;
    // 贴近细线的内容框抓不住，选框至少 12×12（只影响选框与手柄，不改几何与命中）。
    QRectF frameRect() const override;

    // 端点用场景坐标给出；插入与「手绘近直线转生」共用这一入口。
    void setEndpoints(const QPointF& startScene, const QPointF& endScene, const QColor& color, qreal width);
};
