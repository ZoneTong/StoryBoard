#pragma once

#include "ArrowStyle.h"
#include "CanvasItem.h"
#include "ImageFill.h"

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPainterPath>
#include <QPixmap>

enum class ShapeType {
    Rectangle = 0,
    Ellipse,
    Triangle,
    Diamond,
    Star,
    Arrow,
    DoubleArrow,
    Line,
    Callout,
    Sector,
    Arc,
    // 需求 11：常见图形新增 8 种。花括号（BraceLeft/BraceRight）已改为曲线图元，
    // 不再占用 ShapeType（需求 13）。
    Star4,
    Hexagram,
    Heart,
    SineWave,
    FlagWave,
    // 卷形旗帜已替换为卷轴（需求 3），序列化字符串沿用新名 scroll，旧文件 flag_curl 兼容读入。
    Scroll,
    Cube,
    Cylinder,
    // 需求 9：平行四边形（左下内角可编辑，默认 60°）。
    Parallelogram
};

enum class FillStyle {
    None = 0,
    Solid,
    Hachure,
    ReverseHachure,
    CrossHatch,
    GridHatch,
    Honeycomb,
    HorizontalHatch,
    VerticalHatch,
    Brick,
    Dots
};

QList<ShapeType> allShapeTypes();
QString shapeTypeName(ShapeType type);
QString shapeTypeToString(ShapeType type);
ShapeType shapeTypeFromString(const QString& value);
QSizeF defaultShapeSize(ShapeType type);
bool shapeTypeIsFilled(ShapeType type);
bool shapeTypeIsStrokeOnly(ShapeType type);
QIcon makeShapeIcon(ShapeType type, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
// 预置项（圆角矩形 / 45°·60° 直角三角 / 直线箭头）单独画图标，
// 和基础图形并排放在菜单与工具栏时能一眼区分。
QIcon makeShapePresetIcon(const QString& preset, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);

QList<FillStyle> allFillStyles();
QString fillStyleName(FillStyle style);
QString fillStyleToString(FillStyle style);
FillStyle fillStyleFromString(const QString& value);
// 填充纹理的中心线几何，编辑图元卡的工具按钮缩略图与图元绘制共用这一份。
QPainterPath fillPatternCenterLines(FillStyle style, const QRectF& rect);

struct ShapeItemState {
    ShapeType type = ShapeType::Rectangle;
    QColor strokeColor = QColor(30, 41, 59);
    QColor fillColor = QColor(30, 41, 59, 36);
    FillStyle fillStyle = FillStyle::Solid;
    qreal strokeWidth = 4.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    qreal opacity = 1.0;
    qreal cornerRadius = -1.0;
    // 圆角自动态：矩形按短边 16% 取圆角（旧 rounded_rectangle 读入后的形态）。
    bool autoCorner = false;
    // 扇形 / 弧线：起始角与扫过角（度，Qt 坐标：0° 在三点钟方向，逆时针为正）。
    qreal startAngle = 30.0;
    qreal sweepAngle = 120.0;
    // 三角形：底边（内容矩形宽度）两侧的底角，单位为度。角度是真相，高度由两者派生。
    qreal bottomAngle1 = 60.0;
    qreal bottomAngle2 = 60.0;
    // 需求 9：平行四边形左下内角（度）。角度是真相，上边水平偏移由高度与角度派生。
    qreal parallelogramAngle = 60.0;
    // 直线 / 直线箭头 / 曲线两端的箭头样式（旧文件缺字段时按「无」）。
    ArrowStyle startArrow = ArrowStyle::None;
    ArrowStyle endArrow = ArrowStyle::None;
    // 需求 15：图片填充参与撤销快照；QPixmap 隐式共享，不会额外拷贝像素。
    bool hasFillImage = false;
    QPixmap fillImage;
    ImageFill::Mode fillImageMode = ImageFill::Mode::Stretch;
    QPointF pos;
    QSizeF size;

    bool operator==(const ShapeItemState& other) const
    {
        return type == other.type && strokeColor == other.strokeColor && fillColor == other.fillColor &&
               fillStyle == other.fillStyle && qFuzzyCompare(strokeWidth, other.strokeWidth) &&
               strokeStyle == other.strokeStyle && qFuzzyCompare(opacity, other.opacity) &&
               qFuzzyCompare(cornerRadius + 2.0, other.cornerRadius + 2.0) && autoCorner == other.autoCorner &&
               qFuzzyCompare(startAngle + 2.0, other.startAngle + 2.0) &&
               qFuzzyCompare(sweepAngle + 2.0, other.sweepAngle + 2.0) &&
               qFuzzyCompare(bottomAngle1 + 2.0, other.bottomAngle1 + 2.0) &&
               qFuzzyCompare(bottomAngle2 + 2.0, other.bottomAngle2 + 2.0) && startArrow == other.startArrow &&
               qFuzzyCompare(parallelogramAngle + 2.0, other.parallelogramAngle + 2.0) && endArrow == other.endArrow &&
               hasFillImage == other.hasFillImage && fillImage.cacheKey() == other.fillImage.cacheKey() &&
               fillImageMode == other.fillImageMode && pos == other.pos && size == other.size;
    }

    bool operator!=(const ShapeItemState& other) const { return !(*this == other); }
};

class ShapeItem : public CanvasItem {
    Q_OBJECT
  public:
    // 需求 2：带短刻度线的底边是 AB（A 在左、B 在右），对面顶点是 C。
    enum class TriangleVertex { A, B, C };
    explicit ShapeItem(ShapeType type = ShapeType::Rectangle, QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("shape"); }
    QString displayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;
    // 扇形 / 弧线只占内容矩形的一部分，选中框与手柄改按实际轮廓的最小覆盖矩形摆放。
    QRectF frameRect() const override;
    // 三角形"角度为真相"，把外部请求的尺寸先折算成派生尺寸，交给基类一次性应用。
    QSizeF adjustedContentSize(const QSizeF& size) const override;

    ShapeType shapeType() const { return m_type; }
    void setShapeType(ShapeType type);
    QColor strokeColor() const { return m_strokeColor; }
    QColor fillColor() const { return m_fillColor; }
    FillStyle fillStyle() const { return m_fillStyle; }
    qreal strokeWidth() const { return m_strokeWidth; }
    Qt::PenStyle strokeStyle() const { return m_strokeStyle; }
    qreal opacity() const { return m_opacity; }
    qreal itemOpacity() const override { return m_opacity; }
    void setItemOpacity(qreal opacity) override;
    qreal cornerRadius() const { return m_cornerRadius; }
    // 需求 5：矩形用 cornerRadius（px，上限短边一半）；autoCorner 表示按短边 16% 自动取值，
    // 圆角矩形已并入矩形，旧的 rounded_rectangle 读入后即落在这条自动路径上。
    bool autoCorner() const { return m_autoCorner; }
    void setAutoCorner(bool enabled);
    qreal startAngle() const { return m_startAngle; }
    qreal sweepAngle() const { return m_sweepAngle; }
    qreal bottomAngle1() const { return m_bottomAngle1; }
    qreal bottomAngle2() const { return m_bottomAngle2; }
    // 需求 9：平行四边形左下内角（度），钳在 15–165°，默认 60°。
    qreal parallelogramAngle() const { return m_parallelogramAngle; }
    void setParallelogramAngle(qreal angle);
    ArrowStyle startArrow() const { return m_startArrow; }
    // 需求 15：可填充图元支持一张图片垫在填充色之上，独立于 FillStyle。
    bool hasFillImage() const { return !m_fillImage.isNull(); }
    QPixmap fillImage() const { return m_fillImage; }
    ImageFill::Mode fillImageMode() const { return m_fillImageMode; }
    void setFillImage(const QPixmap& image, ImageFill::Mode mode);
    void clearFillImage();
    ArrowStyle endArrow() const { return m_endArrow; }
    void setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);
    void setFill(const QColor& fillColor, FillStyle style);
    void setFillStyle(FillStyle style);
    void setStrokeStyle(Qt::PenStyle style);
    void setOpacity(qreal opacity);
    void setCornerRadius(qreal radius);
    void setArcAngles(qreal startAngle, qreal sweepAngle);
    // 三角形：设置底边两侧的底角（度）。角度优先，高度按底边与角度重算；
    // 两角和被钳制在 175° 以内，保证顶点不会跑出内容矩形。
    void setTriangleAngles(qreal angle1, qreal angle2);
    // 直线 / 直线箭头：两端箭头样式，只影响描线绘制，不改变内容矩形。
    void setArrowStyles(ArrowStyle start, ArrowStyle end);

    // 自定义图形：轮廓来自画布模板，存的是 0–1 单位坐标；没有自定义轮廓时按 shapeType 画。
    bool isCustomShape() const { return !m_customPath.isEmpty(); }
    QString customShapeName() const { return m_customName; }
    QPainterPath customShapePath() const { return m_customPath; }
    void setCustomShape(const QString& name, const QPainterPath& unitPath);

    // 三角形三个顶点（内容矩形局部坐标）：A/B 为底边两端，C 为顶点。
    QPointF triangleVertex(TriangleVertex vertex) const;
    // 需求 2：顶点 C 的句柄位置由三角形几何给出，而不是包围框上边中点。
    QPointF customHandleCenter(int handle, bool* ok) const override;

    ShapeItemState state() const;
    void applyState(const ShapeItemState& state);

    static QPainterPath pathFor(ShapeType type, const QRectF& rect);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle);
    // 当前生效的轮廓：自定义图形按内容矩形缩放单位路径，普通图形带圆角参数。
    QPainterPath currentPath(const QRectF& rect) const;

  protected:
    void paintContent(QPainter* painter) override;
    // 三角形拉伸时保持"角度为真相"：拖高等价于等比改底边，拖底边则反推高度。
    void contentResized() override;
    // 三角形选中时在底边中点两侧补一对短刻度线，提示这里是底边长度手柄。
    void drawSelectionOverlay(QPainter* painter) override;
    qreal framePadding() const override { return qMax<qreal>(22.0, m_strokeWidth + 6.0); }

  private:
    // 直线箭头：把两端箭头几何并入描带，命中与遮挡才算得上"头也是线的一部分"。
    QPainterPath pathWithArrowHeads(const QPainterPath& base) const;
    ShapeType m_type = ShapeType::Rectangle;
    QColor m_strokeColor = QColor(30, 41, 59);
    QColor m_fillColor = QColor(30, 41, 59, 36);
    FillStyle m_fillStyle = FillStyle::Solid;
    qreal m_strokeWidth = 4.0;
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
    qreal m_opacity = 1.0;
    // < 0 表示未显式指定圆角；矩形在 autoCorner 打开时按短边 16% 自动取值。
    qreal m_cornerRadius = -1.0;
    bool m_autoCorner = false;
    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
    qreal m_bottomAngle1 = 60.0;
    qreal m_bottomAngle2 = 60.0;
    qreal m_parallelogramAngle = 60.0;
    QPixmap m_fillImage;
    ImageFill::Mode m_fillImageMode = ImageFill::Mode::Stretch;
    ArrowStyle m_startArrow = ArrowStyle::None;
    ArrowStyle m_endArrow = ArrowStyle::None;
    // 上一次同步后的尺寸，用于区分"拖高"与"拖底边"，避免互相覆盖。
    QSizeF m_triangleSyncSize;
    bool m_syncingTriangle = false;
    QString m_customName;
    QPainterPath m_customPath;
};