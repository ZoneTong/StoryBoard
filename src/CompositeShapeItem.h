#pragma once

#include "CanvasItem.h"

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QPainterPath>

// 布尔运算得到的复合图元：内部按“优先级从低到高”保存若干图层，
// 每层是一块路径 + 自己的填充色/描边色。逐层绘制时高层后画，
// 重叠区域自然显示高优先级图元（图层更高 / 同层 height 更大）的颜色，
// 被盖住的下层轮廓也被高层填充覆盖。
class CompositeShapeItem : public CanvasItem {
  public:
    struct Layer {
        QPainterPath path; // 内容区局部坐标（0,0 为左上角）
        QColor fill;
        QColor stroke;
        qreal strokeWidth = 1.0;
        bool filled = true;
    };

    explicit CompositeShapeItem(const QList<Layer>& layers = QList<Layer>());

    QString kind() const override { return QStringLiteral("composite"); }
    QString displayName() const override;
    QString typeDisplayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    int layerCount() const { return m_layers.size(); }
    const QList<Layer>& layers() const { return m_layers; }

    // 按当前内容尺寸缩放后的整体轮廓，供命中与遮挡判断使用。
    QPainterPath paintedShape() const override;
    QPainterPath outlineShape() const override;

    // 路径 → JSON 元素数组（布尔运算结果随画布文件保存与撤销一起走）。
    static QJsonArray pathToJson(const QPainterPath& path);
    static QPainterPath pathFromJson(const QJsonArray& elements);

  protected:
    void paintContent(QPainter* painter) override;

  private:
    void updateBoundsFromLayers();

    QList<Layer> m_layers;
    QSizeF m_designSize{1.0, 1.0};
    // 内容尺寸变化时不改路径，绘制阶段按比例缩放，避免反复量化误差累积。
    QTransform layerTransform() const;
};
