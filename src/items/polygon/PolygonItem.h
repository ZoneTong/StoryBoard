#pragma once

#include "items/properties/ImageFill.h"
#include "items/shapes/ShapeAbstractItem.h"

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QPainterPath>
#include <QPixmap>
#include <Qt>

// 多边形图元：合并 / 分割等布尔运算的产物，派生自有面积图形（ShapeAbstractItem），
// 但自身没有单一 ShapeType——内部按“优先级从低到高”保存若干图层，每层是一块
// 路径 + 自己的填充色/描线色。填充取优先级最高且不透明的颜色，高层填充透明时
// 退到次优先级；内部交界轮廓不描线，由填充色覆盖。
class PolygonItem : public ShapeAbstractItem {
  public:
    // 图层来源决定填充 / 描线的应用方式：
    // 有面积图形参与填充与描边；直线 / 曲线只保留线宽带，不再二次描线；文字只填字形。
    enum class LayerRole { FILLED_AREA = 0, STROKE_BAND = 1, TEXT = 2 };

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

    QString Kind() const override { return QStringLiteral("polygon"); }
    // 多边形没有内置轮廓，shapeType 只用于形状派生的公共分支（填充、命中已单独覆写）。
    ShapeType shapeType() const override { return ShapeType::POLYGON; }
    // 访问者入口：按图层取几何的调用方走 VisitPolygon，不再强转本类（D16）。
    void Accept(CanvasItemVisitor& visitor) override;
    // 派生自 ShapeAbstractItem 但不受形状类型语义约束：凡按 shapeType 分派的入口都先看这一位。
    QString DisplayName() const override;
    QString TypeDisplayName() const override;
    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;
    // 命中与轮廓取所有图层并集（ShapeAbstractItem 按单一 ShapeType 生成路径，不能沿用）。
    QPainterPath shape() const override;
    QPainterPath CurrentPath(const QRectF& rect) const override;
    QPainterPath GeometryPaintedShape() const override;
    QPainterPath GeometryOutline() const override;

    int LayerCount() const { return m_layers.size(); }
    const QList<Layer>& Layers() const { return m_layers; }
    // 是否有可填充的区域层（孔洞不算）：布尔运算与线条分割按它判断参与资格。
    bool HasFillAreaLayer() const;

    // 需求 11：把路径里每个子路径显式闭合（布尔交集 / 拆块与序列化重建的
    // 路径不带 closeSubpath 标记，描边时收尾一条边会缺失）。开放笔画请勿使用。
    static QPainterPath ClosedSubpaths(const QPainterPath& path);

    // 多边形共用的填充 / 描线属性：填充作用于有面积图形与文字字形，
    // 描线颜色同时同步直线 / 曲线的线宽带颜色；样式与宽度只作用于有面积轮廓。
    QColor FillColor() const override;
    QColor StrokeColor() const override;
    qreal StrokeWidth() const override;
    FillStyle fillStyle() const override { return m_fillStyle; }
    Qt::PenStyle StrokeStyle() const override { return m_strokeStyle; }
    void SetFillColor(const QColor& color) override;
    void SetFill(const QColor& color, FillStyle style) override;
    void SetFillStyle(FillStyle style) override;
    void SetStrokeColor(const QColor& color) override;
    void SetStrokeWidth(qreal width) override;
    void SetStroke(const QColor& color, qreal width);
    // 样式三件套与单形状同签名：合并后仍按「填充 + 描线」统一套用。
    void SetStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth) override;
    void SetStrokeStyle(Qt::PenStyle style) override;
    // 需求 15：图片填充只作用于 FilledArea 层围成的区域，孔洞留空；不参与合并与导出。
    bool HasFillImage() const override { return !m_fillImage.isNull(); }
    QPixmap FillImage() const override { return m_fillImage; }
    ImageFill::Mode FillImageMode() const override { return m_fillImageMode; }
    void SetFillImage(const QPixmap& image, ImageFill::Mode mode) override;
    void ClearFillImage() override;

    // 样式撤销快照：多边形没有 ShapeType，用 polygon 标记与普通图形区分。
    ShapeItemState State() const override;
    void ApplyState(const ShapeItemState& state) override;

    // 把某层的局部路径映射到图元坐标；布尔运算需要按层角色分别取几何。
    QPainterPath MapLayerPath(const QPainterPath& localPath) const;

    // 路径 → JSON 元素数组（布尔运算结果随画布文件保存与撤销一起走）。
    static QJsonArray PathToJson(const QPainterPath& path);
    static QPainterPath PathFromJson(const QJsonArray& elements);

  protected:
    // 基类纯虚钩子：多边形的轮廓来自图层，currentPath 已覆写。
    QPainterPath ShapePath(const QRectF& rect) const override
    {
        Q_UNUSED(rect);
        return QPainterPath();
    }
    void PaintContent(QPainter* painter) override;
    // 选中框就是内容矩形，取景余量沿用画布图元的默认值（不跟着形状类型的 22px）。
    QRectF FrameRect() const override { return CanvasItem::FrameRect(); }
    qreal FramePadding() const override { return CanvasItem::FramePadding(); }

  private:
    void UpdateBoundsFromLayers();

    QList<Layer> m_layers;
    QSizeF m_baseSize{1.0, 1.0};
    // 内容尺寸变化时不改路径，绘制阶段按比例缩放，避免反复量化误差累积。
    QTransform LayerTransform() const;
};
