#pragma once

#include "CanvasItem.h"
#include "ImageFill.h"
#include "ShapeItem.h"

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QPainterPath>
#include <QPixmap>
#include <Qt>

// 合并图元得到的多边形：内部按“优先级从低到高”保存若干图层，
// 每层是一块路径 + 自己的填充色/描线色。填充取优先级最高且不透明的颜色，
// 高层填充透明时退到次优先级；内部交界轮廓不描线，由填充色覆盖。
class CompositeShapeItem : public CanvasItem {
  public:
    // 图层来源决定填充 / 描线的应用方式：
    // 有面积图形参与填充与描边；直线 / 曲线只保留线宽带，不再二次描线；文字只填字形。
    enum class LayerRole { FilledArea = 0, StrokeBand = 1, Text = 2 };

    struct Layer {
        QPainterPath path; // 内容区局部坐标（0,0 为左上角）
        QColor fill;
        QColor stroke;
        qreal strokeWidth = 1.0;
        bool filled = true;
        LayerRole role = LayerRole::FilledArea;
    };

    explicit CompositeShapeItem(const QList<Layer>& layers = QList<Layer>(), FillStyle fillStyle = FillStyle::Solid,
                                Qt::PenStyle strokeStyle = Qt::SolidLine);

    QString kind() const override { return QStringLiteral("polygon"); }
    QString displayName() const override;
    QString typeDisplayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;

    int layerCount() const { return m_layers.size(); }
    const QList<Layer>& layers() const { return m_layers; }

    // 多边形共用的填充 / 描线属性：填充作用于有面积图形与文字字形，
    // 描线颜色同时同步直线 / 曲线的线宽带颜色；样式与宽度只作用于有面积轮廓。
    QColor fillColor() const;
    QColor strokeColor() const;
    qreal strokeWidth() const;
    FillStyle fillStyle() const { return m_fillStyle; }
    Qt::PenStyle strokeStyle() const { return m_strokeStyle; }
    void setFill(const QColor& color, FillStyle style);
    void setFillStyle(FillStyle style);
    void setStroke(const QColor& color, qreal width);
    void setStrokeStyle(Qt::PenStyle style);
    // 需求 15：图片填充只作用于 FilledArea 层围成的区域，孔洞留空；不参与合并与导出。
    bool hasFillImage() const { return !m_fillImage.isNull(); }
    QPixmap fillImage() const { return m_fillImage; }
    ImageFill::Mode fillImageMode() const { return m_fillImageMode; }
    void setFillImage(const QPixmap& image, ImageFill::Mode mode);
    void clearFillImage();

    // 按当前内容尺寸缩放后的整体轮廓，供命中与遮挡判断使用。
    QPainterPath paintedShape() const override;
    QPainterPath outlineShape() const override;
    // 把某层的局部路径映射到图元坐标；布尔运算需要按层角色分别取几何。
    QPainterPath mapLayerPath(const QPainterPath& localPath) const;

    // 路径 → JSON 元素数组（布尔运算结果随画布文件保存与撤销一起走）。
    static QJsonArray pathToJson(const QPainterPath& path);
    static QPainterPath pathFromJson(const QJsonArray& elements);

  protected:
    void paintContent(QPainter* painter) override;

  private:
    void updateBoundsFromLayers();

    QList<Layer> m_layers;
    QSizeF m_designSize{1.0, 1.0};
    FillStyle m_fillStyle = FillStyle::Solid;
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
    QPixmap m_fillImage;
    ImageFill::Mode m_fillImageMode = ImageFill::Mode::Stretch;
    // 内容尺寸变化时不改路径，绘制阶段按比例缩放，避免反复量化误差累积。
    QTransform layerTransform() const;
};
