#pragma once

#include "ImageFill.h"
#include "ShapeItem.h"

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QPainterPath>
#include <QPixmap>
#include <Qt>

// 多边形图元：合并 / 分割等布尔运算的产物，派生自有面积图形（ShapeItem），
// 但自身没有单一 ShapeType——内部按“优先级从低到高”保存若干图层，每层是一块
// 路径 + 自己的填充色/描线色。填充取优先级最高且不透明的颜色，高层填充透明时
// 退到次优先级；内部交界轮廓不描线，由填充色覆盖。
class PolygonItem : public ShapeItem {
  public:
    // 图层来源决定填充 / 描线的应用方式：
    // 有面积图形参与填充与描边；直线 / 曲线只保留线宽带，不再二次描线；文字只填字形。
    enum class LayerRole { FILLED_AREA = 0, STROKE_BAND = 1, TEXT = 2 };
    // 基类只读视图的覆写：把内部 Layer 映射成 ShapeLayer。
    int shapeLayerCount() const override { return m_layers.size(); }
    ShapeLayer shapeLayer(int index) const override;
    QPainterPath mapShapeLayerPath(const ShapeLayer& layer) const override;

    struct Layer {
        QPainterPath path; // 内容区局部坐标（0,0 为左上角）
        QColor fill;
        QColor stroke;
        qreal strokeWidth = 1.0;
        bool filled = true;
        LayerRole role = LayerRole::FILLED_AREA;
    };

    explicit PolygonItem(const QList<Layer>& layers = QList<Layer>(), FillStyle fillStyle = FillStyle::SOLID,
                         Qt::PenStyle strokeStyle = Qt::SolidLine);

    QString kind() const override { return QStringLiteral("polygon"); }
    // 多边形没有内置轮廓，shapeType 只用于形状派生的公共分支（填充、命中已单独覆写）。
    ShapeType shapeType() const override { return ShapeType::POLYGON; }
    // 派生自 ShapeItem 但不受形状类型语义约束：凡按 shapeType 分派的入口都先看这一位。
    QString displayName() const override;
    QString typeDisplayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    // 命中与轮廓取所有图层并集（ShapeItem 按单一 ShapeType 生成路径，不能沿用）。
    QPainterPath shape() const override;
    QPainterPath currentPath(const QRectF& rect) const override;
    QPainterPath paintedShape() const override;
    QPainterPath outlineShape() const override;

    int layerCount() const { return m_layers.size(); }
    const QList<Layer>& layers() const { return m_layers; }

    // 需求 11：把路径里每个子路径显式闭合（布尔交集 / 拆块与序列化重建的
    // 路径不带 closeSubpath 标记，描边时收尾一条边会缺失）。开放笔画请勿使用。
    static QPainterPath closedSubpaths(const QPainterPath& path);

    // 多边形共用的填充 / 描线属性：填充作用于有面积图形与文字字形，
    // 描线颜色同时同步直线 / 曲线的线宽带颜色；样式与宽度只作用于有面积轮廓。
    QColor fillColor() const override;
    QColor strokeColor() const override;
    qreal strokeWidth() const override;
    FillStyle fillStyle() const override { return m_fillStyle; }
    Qt::PenStyle strokeStyle() const override { return m_strokeStyle; }
    void setFill(const QColor& color, FillStyle style) override;
    void setFillStyle(FillStyle style) override;
    void setStroke(const QColor& color, qreal width);
    // 样式三件套与单形状同签名：合并后仍按「填充 + 描线」统一套用。
    void setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth) override;
    void setStrokeStyle(Qt::PenStyle style) override;
    // 需求 15：图片填充只作用于 FilledArea 层围成的区域，孔洞留空；不参与合并与导出。
    bool hasFillImage() const override { return !m_fillImage.isNull(); }
    QPixmap fillImage() const override { return m_fillImage; }
    ImageFill::Mode fillImageMode() const override { return m_fillImageMode; }
    void setFillImage(const QPixmap& image, ImageFill::Mode mode) override;
    void clearFillImage() override;

    // 样式撤销快照：多边形没有 ShapeType，用 polygon 标记与普通图形区分。
    ShapeItemState state() const override;
    void applyState(const ShapeItemState& state) override;

    // 把某层的局部路径映射到图元坐标；布尔运算需要按层角色分别取几何。
    QPainterPath mapLayerPath(const QPainterPath& localPath) const;

    // 路径 → JSON 元素数组（布尔运算结果随画布文件保存与撤销一起走）。
    static QJsonArray pathToJson(const QPainterPath& path);
    static QPainterPath pathFromJson(const QJsonArray& elements);

  protected:
    // 基类纯虚钩子：多边形的轮廓来自图层，currentPath 已覆写。
    QPainterPath shapePath(const QRectF& rect) const override
    {
        Q_UNUSED(rect);
        return QPainterPath();
    }
    void paintContent(QPainter* painter) override;
    // 选中框就是内容矩形，取景余量沿用画布图元的默认值（不跟着形状类型的 22px）。
    QRectF frameRect() const override { return CanvasItem::frameRect(); }
    qreal framePadding() const override { return CanvasItem::framePadding(); }

  private:
    void updateBoundsFromLayers();

    QList<Layer> m_layers;
    QSizeF m_designSize{1.0, 1.0};
    FillStyle m_fillStyle = FillStyle::SOLID;
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
    QPixmap m_fillImage;
    ImageFill::Mode m_fillImageMode = ImageFill::Mode::STRETCH;
    // 内容尺寸变化时不改路径，绘制阶段按比例缩放，避免反复量化误差累积。
    QTransform layerTransform() const;
};
