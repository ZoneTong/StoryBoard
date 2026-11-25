#pragma once

#include "app/I18n.h"
#include "items/SectorBase.h"
#include "items/lines/LineAbstractItem.h"

#include <QRectF>
#include <QVector>

// 角度线图元：一段“两射线 + 共用顶点”的角标注。派生于描线图元与椭圆截段基类：
// 起止角决定两条射线方向，两条半轴决定射线长度，顶点是椭圆中心。
// 箭头复用描线基类的首尾两枚箭头字段（落在两条射线的外端，顶点不带箭头）。
class AngleItem : public LineAbstractItem, public SectorBase {
    Q_OBJECT
  public:
    explicit AngleItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("angle"); }
    QString DisplayName() const override;
    QRectF FrameRect() const override;
    QPointF RotationOrigin() const override;
    QPointF CenterScenePoint() const override;
    QSizeF EllipseSize() const { return EllipseSizeFor(ContentSize()); }
    bool ArcAxisWritable() const override { return true; }

    void SetAngles(qreal startDeg, qreal sweepDeg);
    void SetArcAngles(qreal startDeg, qreal sweepDeg) override;
    void SetArcAxes(qreal majorAxis, qreal minorAxis);

    // 插入入口：顶点与两条射线端点由椭圆框 + 起始角 / 扫过角决定。
    void SetAngleGeometry(const QPointF& topLeftScene, const QSizeF& size, qreal startDeg, qreal sweepDeg,
                          const QColor& color, qreal width);

    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;

    HandleSpecList CustomHandles() const override;
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;
    void EndHandleDrag() override;

    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;

  protected:
    void ArcAnglesChanged() override;
    void DrawSelectionOverlay(QPainter* painter) override;
    QList<QRectF> AngleReadoutRects() const override;
    qreal FramePadding() const override;

  private:
    void Rebuild();
    void RebuildWithFrame(const QRectF& frameScene);
    // 顶点 + 两条射线外端，顺序固定为 [起始端, 顶点, 结束端]，箭头才能落在两条射线的外端。
    QVector<QPointF> AnglePoints(const QRectF& frameScene) const;
};
