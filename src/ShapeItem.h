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
    Callout,
    Sector,
    // 需求 11：常见图形新增 8 种。花括号（BraceLeft/BraceRight）已改为曲线图元，
    // 不再占用 ShapeType（需求 13）。
    Star4,
    Hexagram,
    Heart,
    FlagWave,
    // 卷形旗帜已替换为卷轴（需求 3），序列化字符串沿用新名 scroll，旧文件 flag_curl 兼容读入。
    Scroll,
    Cube,
    Cylinder,
    // 需求 9：平行四边形（左下内角可编辑，默认 60°）。
    Parallelogram,
    // 多边形（合并 / 分割产物）没有内置轮廓，仅为满足 shapeType() 的返回值占位。
    Polygon
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

QIcon makeShapeIcon(ShapeType type, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
// 预置项（圆角矩形 / 45°·60° 直角三角 / 直线箭头）单独画图标，
// 和基础图形并排放在菜单与工具栏时能一眼区分。
QIcon makeShapePresetIcon(const QString& preset, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);

QList<FillStyle> allFillStyles();
QString fillStyleName(FillStyle style);
QString fillStyleToString(FillStyle style);
FillStyle fillStyleFromString(const QString& value);
// 填充纹理的中心线几何，属性编辑面板的工具按钮缩略图与图元绘制共用这一份。
QPainterPath fillPatternCenterLines(FillStyle style, const QRectF& rect);

// 多边形图元的内部图层（合并 / 分割产物）：基类只暴露只读视图，
// 具体存储仍在 PolygonItem，避免上层按叶子类强转。
enum class ShapeLayerRole { FILL_AREA = 0, STROKE_BAND = 1, TEXT = 2 };

struct ShapeLayer {
    QPainterPath path; // 内容区局部坐标（0,0 为左上角）
    QColor fill;
    QColor stroke;
    qreal strokeWidth = 1.0;
    bool filled = true;
    ShapeLayerRole role = ShapeLayerRole::FILL_AREA;
};

struct ShapeItemState {
    ShapeType type = ShapeType::Rectangle;
    QColor strokeColor = QColor(30, 41, 59);
    QColor fillColor = QColor(30, 41, 59, 36);
    FillStyle fillStyle = FillStyle::Solid;
    qreal strokeWidth = 4.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    qreal opacity = 1.0;
    qreal cornerRadius = 0.0;
    // 扇形 / 弧线：起始角与扫过角（度，Qt 坐标：0° 在三点钟方向，逆时针为正）。
    qreal startAngle = 30.0;
    qreal sweepAngle = 120.0;
    // 三角形：底边（内容矩形宽度）两侧的底角，单位为度。角度是真相，高度由两者派生。
    qreal bottomAngle1 = 60.0;
    qreal bottomAngle2 = 60.0;
    // 需求 9：平行四边形左下内角（度）。角度是真相，上边水平偏移由高度与角度派生。
    qreal parallelogramAngle = 60.0;
    // 直线 / 直线箭头 / 曲线两端的箭头样式（旧文件缺字段时按「无」）。
    // 需求 15：图片填充参与撤销快照；QPixmap 隐式共享，不会额外拷贝像素。
    bool hasFillImage = false;
    QPixmap fillImage;
    ImageFill::Mode fillImageMode = ImageFill::Mode::Stretch;
    QPointF pos;
    QSizeF size;
    // 多边形（合并 / 分割产物）没有 ShapeType，用这一位与普通图形区分，
    // 避免样式快照把矩形与多边形看成同一种状态。
    bool polygon = false;

    bool operator==(const ShapeItemState& other) const
    {
        return type == other.type && strokeColor == other.strokeColor && fillColor == other.fillColor &&
               fillStyle == other.fillStyle && qFuzzyCompare(strokeWidth, other.strokeWidth) &&
               strokeStyle == other.strokeStyle && qFuzzyCompare(opacity, other.opacity) &&
               qFuzzyCompare(cornerRadius + 2.0, other.cornerRadius + 2.0) &&
               qFuzzyCompare(startAngle + 2.0, other.startAngle + 2.0) &&
               qFuzzyCompare(sweepAngle + 2.0, other.sweepAngle + 2.0) &&
               qFuzzyCompare(bottomAngle1 + 2.0, other.bottomAngle1 + 2.0) &&
               qFuzzyCompare(bottomAngle2 + 2.0, other.bottomAngle2 + 2.0) &&
               qFuzzyCompare(parallelogramAngle + 2.0, other.parallelogramAngle + 2.0) &&
               hasFillImage == other.hasFillImage && fillImage.cacheKey() == other.fillImage.cacheKey() &&
               fillImageMode == other.fillImageMode && pos == other.pos && size == other.size &&
               polygon == other.polygon;
    }

    bool operator!=(const ShapeItemState& other) const { return !(*this == other); }
};

class ShapeItem : public CanvasItem {
    Q_OBJECT
  public:
    // 需求 2：带短刻度线的底边是 AB（A 在左、B 在右），对面顶点是 C。
    enum class TriangleVertex { A, B, C };
    explicit ShapeItem(QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("shape"); }
    // 每种图形由 src/shapes/ 下的具体类给出身份；基类不再保存 ShapeType。
    virtual ShapeType shapeType() const = 0;
    // 属性表：填充 / 描线六件套为图形族公共项，各子类再按需要追加。
    ItemPropertySet properties() const override;
    QVariant propertyValue(ItemProperty property) const override;
    void setPropertyValue(ItemProperty property, const QVariant& value) override;
    // 多边形（PolygonItem）派生自本类，但没有内置轮廓：按 shapeType 分派的入口
    // 都要先看这一位。
    virtual bool isPolygon() const { return false; }
    // 内部图层视图：普通图形没有图层（0 层），多边形返回自己的层。
    virtual int shapeLayerCount() const { return 0; }
    virtual ShapeLayer shapeLayer(int index) const;
    // 图层路径映射到图元坐标；多边形按内容尺寸缩放，普通图形默认恒等。
    virtual QPainterPath mapShapeLayerPath(const ShapeLayer& layer) const;
    // 是否有可填充的区域层（孔洞不算）。
    bool hasShapeFillArea() const;
    QString displayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;
    QRectF frameRect() const override;
    QSizeF adjustedContentSize(const QSizeF& size) const override;

    // 样式访问器对多边形开放覆写：多边形的颜色来自内部图层，而不是单一字段。
    virtual QColor strokeColor() const { return m_strokeColor; }
    virtual QColor fillColor() const { return m_fillColor; }
    virtual FillStyle fillStyle() const { return m_fillStyle; }
    virtual qreal strokeWidth() const { return m_strokeWidth; }
    virtual Qt::PenStyle strokeStyle() const { return m_strokeStyle; }
    qreal itemOpacity() const override { return m_opacity; }
    void setItemOpacity(qreal opacity) override;
    qreal cornerRadius() const { return m_cornerRadius; }
    qreal startAngle() const { return m_startAngle; }
    qreal sweepAngle() const { return m_sweepAngle; }
    qreal bottomAngle1() const { return m_bottomAngle1; }
    qreal bottomAngle2() const { return m_bottomAngle2; }
    qreal parallelogramAngle() const { return m_parallelogramAngle; }
    virtual void setParallelogramAngle(qreal angle);
    // 平行四边形用 Apex 句柄改内角；矩形锁 90°，不提供该句柄。
    virtual bool apexAngleHandle() const { return false; }
    void setAngleReadoutVisible(bool visible);
    bool angleReadoutVisible() const { return m_angleReadoutVisible; }
    virtual bool hasFillImage() const { return !m_fillImage.isNull(); }
    virtual QPixmap fillImage() const { return m_fillImage; }
    QString fillImageId() const { return m_fillImageId; }
    void setFillImageId(const QString& id) { m_fillImageId = id; }
    virtual ImageFill::Mode fillImageMode() const { return m_fillImageMode; }
    virtual void setFillImage(const QPixmap& image, ImageFill::Mode mode);
    virtual void clearFillImage();
    virtual void setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);
    virtual void setFill(const QColor& fillColor, FillStyle style);
    virtual void setFillStyle(FillStyle style);
    virtual void setStrokeStyle(Qt::PenStyle style);
    void setCornerRadius(qreal radius);
    void setArcAngles(qreal startAngle, qreal sweepAngle);
    // 三角形：设置底边两侧的底角（度）。角度优先，高度按底边与角度重算；
    // 两角和被钳制在 175° 以内，保证顶点不会跑出内容矩形。
    void setTriangleAngles(qreal angle1, qreal angle2);
    // 自定义图形：轮廓来自画布模板，存的是 0–1 单位坐标；没有自定义轮廓时按子类轮廓画。
    bool isCustomShape() const { return !m_customPath.isEmpty(); }
    QString customShapeName() const { return m_customName; }
    QPainterPath customShapePath() const { return m_customPath; }
    void setCustomShape(const QString& name, const QPainterPath& unitPath);

    // 三角形三个顶点（内容矩形局部坐标）：A/B 为底边两端，C 为顶点。
    QPointF triangleVertex(TriangleVertex vertex) const;
    // 需求 2：顶点 C 的句柄位置由三角形几何给出，而不是包围框上边中点。
    QPointF customHandleCenter(int handle, bool* ok) const override;

    // 样式撤销快照：多边形覆写后只记录样式与位置，不写 ShapeType。
    virtual ShapeItemState state() const;
    virtual void applyState(const ShapeItemState& state);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle);
    // 当前生效的轮廓：自定义图形按内容矩形缩放单位路径，普通图形交给具体类实现。
    virtual QPainterPath currentPath(const QRectF& rect) const;
    // 插入 / 状态与 JSON 缺省时的初始内容尺寸；数值只存在于各子类，基类默认 2 格正方形。
    virtual QSizeF defaultContentSize() const;

  protected:
    // 具体图形的内置轮廓（不含自定义模板与矩形圆角自动态哨兵）。
    virtual QPainterPath shapePath(const QRectF& rect) const = 0;
    // 子类可覆写的状态 / JSON 增量。
    virtual void writeState(ShapeItemState& state) const;
    virtual void readState(const ShapeItemState& state);
    virtual void writeJsonExtra(QJsonObject& json) const;
    virtual void readJsonExtra(const QJsonObject& json);
    void paintContent(QPainter* painter) override;
    void contentResized() override;
    void drawSelectionOverlay(QPainter* painter) override;
    qreal framePadding() const override { return qMax<qreal>(22.0, m_strokeWidth + 6.0); }

  private:
    QColor m_strokeColor = QColor(30, 41, 59);
    QColor m_fillColor = QColor(30, 41, 59, 36);
    FillStyle m_fillStyle = FillStyle::Solid;
    qreal m_strokeWidth = 4.0;
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
    qreal m_opacity = 1.0;
    // 圆角半径，单位画布 px；0 表示直角。
    qreal m_cornerRadius = 0.0;
    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
    qreal m_bottomAngle1 = 60.0;
    qreal m_bottomAngle2 = 60.0;
    qreal m_parallelogramAngle = 60.0;
    QPixmap m_fillImage;
    QString m_fillImageId;
    ImageFill::Mode m_fillImageMode = ImageFill::Mode::Stretch;
    // 上一次同步后的尺寸，用于区分「拖高」与「拖底边」，避免互相覆盖。
    QSizeF m_triangleSyncSize;
    bool m_syncingTriangle = false;
    bool m_angleReadoutVisible = false;
    QString m_customName;
    QPainterPath m_customPath;
};

// 图形工厂（定义在 shapes/ShapeFactory.cpp）：按 ShapeType 建出具体子类。
ShapeItem* createShapeItem(ShapeType type);
