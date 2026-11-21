#pragma once

#include "ArcAngleAttributes.h"
#include "ArrowStyle.h"
#include "CanvasItem.h"
#include "FillAttributes.h"
#include "ImageFill.h"
#include "StrokeAttributes.h"

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPainterPath>
#include <QPixmap>

enum class ShapeType {
    RECTANGLE = 0,
    ELLIPSE,
    TRIANGLE,
    STAR,
    ARROW,
    DOUBLE_ARROW,
    CALLOUT,
    SECTOR,
    // 需求 11：常见图形新增 8 种。花括号（BraceLeft/BraceRight）已改为曲线图元，
    // 不再占用 ShapeType（需求 13）。
    STAR4,
    HEXAGRAM,
    HEART,
    FLAG_WAVE,
    // 卷形旗帜已替换为卷轴（需求 3），序列化字符串沿用新名 scroll，旧文件 flag_curl 兼容读入。
    SCROLL,
    CUBE,
    CYLINDER,
    // 需求 9：平行四边形（左下内角可编辑，默认 60°）。
    PARALLELOGRAM,
    // 多边形（合并 / 分割产物）没有内置轮廓，仅为满足 shapeType() 的返回值占位。
    POLYGON,
    // 菱形：平行四边形家族里四边恒等的特例，由 DiamondItem 负责轮廓与固定角。
    // 追加在末尾，避免既有枚举值发生位移。
    DIAMOND
};

QList<ShapeType> AllShapeTypes();
QString ShapeTypeName(ShapeType type);
QString ShapeTypeToString(ShapeType type);
ShapeType ShapeTypeFromString(const QString& value);
QSizeF DefaultShapeSize(ShapeType type);
bool ShapeTypeIsFilled(ShapeType type);

QIcon MakeShapeIcon(ShapeType type, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);
// 预置项（圆角矩形 / 45°·60° 直角三角 / 直线箭头）单独画图标，
// 和基础图形并排放在菜单与工具栏时能一眼区分。
QIcon MakeShapePresetIcon(const QString& preset, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);

QList<FillStyle> AllFillStyles();
QString FillStyleName(FillStyle style);
// 填充纹理的中心线几何，编辑面板的工具按钮缩略图与图元绘制共用这一份。
QPainterPath FillPatternCenterLines(FillStyle style, const QRectF& rect);

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
    // 填充 / 描线两组属性直接复用属性基类的快照，不再在图形状态里重复字段。
    FillAttributesState fill;
    StrokeAttributesState stroke;
    qreal opacity = 1.0;
    qreal cornerRadius = 0.0;
    // 扇形：起始角与扫过角（度，Qt 坐标：0° 在三点钟方向，逆时针为正）。
    ArcAngleAttributesState arc;
    // 三角形：底边（内容矩形宽度）两侧的底角，单位为度。角度是真相，高度由两者派生。
    qreal bottomAngle1 = 60.0;
    qreal bottomAngle2 = 60.0;
    // 需求 9：平行四边形左下内角（度）。角度是真相，上边水平偏移由高度与角度派生。
    qreal parallelogramAngle = 60.0;
    QPointF pos;
    QSizeF size;

    bool operator==(const ShapeItemState& other) const
    {
        return fill == other.fill && stroke == other.stroke && qFuzzyCompare(opacity, other.opacity) &&
               qFuzzyCompare(cornerRadius + 2.0, other.cornerRadius + 2.0) && arc == other.arc &&
               qFuzzyCompare(bottomAngle1 + 2.0, other.bottomAngle1 + 2.0) &&
               qFuzzyCompare(bottomAngle2 + 2.0, other.bottomAngle2 + 2.0) &&
               qFuzzyCompare(parallelogramAngle + 2.0, other.parallelogramAngle + 2.0) && pos == other.pos &&
               size == other.size;
    }

    bool operator!=(const ShapeItemState& other) const { return !(*this == other); }
};

class ShapeItem : public CanvasItem, public FillAttributes, public StrokeAttributes, public ArcAngleAttributes {
    Q_OBJECT
  public:
    // 需求 2：带短刻度线的底边是 AB（A 在左、B 在右），对面顶点是 C。
    enum class TriangleVertex { A, B, C };
    explicit ShapeItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("shape"); }
    // 每种图形由 src/shapes/ 下的具体类给出身份；基类不再保存 ShapeType。
    virtual ShapeType shapeType() const = 0;
    // 属性表：填充 / 描线六件套为图形族公共项，各子类再按需要追加。
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    // 内部图层视图：普通图形没有图层（0 层），多边形返回自己的层。
    virtual int ShapeLayerCount() const { return 0; }
    virtual ShapeLayer shapeLayer(int index) const;
    // 图层路径映射到图元坐标；多边形按内容尺寸缩放，普通图形默认恒等。
    virtual QPainterPath MapShapeLayerPath(const ShapeLayer& layer) const;
    // 是否有可填充的区域层（孔洞不算）。
    bool HasShapeFillArea() const;
    QString DisplayName() const override;
    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath OutlineShape() const override;
    QPainterPath PaintedShape() const override;
    QRectF FrameRect() const override;
    QSizeF AdjustedContentSize(const QSizeF& size) const override;

    // 填充 / 描线的读写接口由两个属性基类提供；多边形按内部图层覆写其中若干条。
    // 这里只保留图形特有的组合入口与几何属性。
    qreal ItemOpacity() const override { return m_opacity; }
    void SetItemOpacity(qreal opacity) override;
    qreal CornerRadius() const { return m_cornerRadius; }
    qreal BottomAngle1() const { return m_bottomAngle1; }
    qreal BottomAngle2() const { return m_bottomAngle2; }
    qreal ParallelogramAngle() const { return m_parallelogramAngle; }
    virtual void SetParallelogramAngle(qreal angle);
    // 平行四边形用 Apex 句柄改内角；矩形锁 90°，不提供该句柄。
    virtual bool ApexAngleHandle() const { return false; }
    // 平行四边形用上边中点的倾斜旋钮连续改内角（可跨过 90°）；菱形四边恒等，不提供。
    virtual bool SkewHandleEnabled() const { return false; }
    void SetAngleReadoutVisible(bool visible);
    bool AngleReadoutVisible() const { return m_angleReadoutVisible; }
    virtual void SetStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);
    void SetCornerRadius(qreal radius);
    // 起始角 / 扫过角由 ArcAngleAttributes 提供；扇形与弧线共用同一份语义。
    // 三角形：设置底边两侧的底角（度）。角度优先，高度按底边与角度重算；
    // 两角和被钳制在 175° 以内，保证顶点不会跑出内容矩形。
    void SetTriangleAngles(qreal angle1, qreal angle2);
    // 自定义图形：轮廓来自画布模板，存的是 0–1 单位坐标；没有自定义轮廓时按子类轮廓画。
    bool IsCustomShape() const { return !m_customPath.isEmpty(); }
    QString CustomShapeName() const { return m_customName; }
    QPainterPath CustomShapePath() const { return m_customPath; }
    void SetCustomShape(const QString& name, const QPainterPath& unitPath);

    // 三角形三个顶点（内容矩形局部坐标）：A/B 为底边两端，C 为顶点。
    QPointF triangleVertex(TriangleVertex vertex) const;
    // 需求 2：顶点 C 的句柄位置由三角形几何给出，而不是包围框上边中点。
    QPointF CustomHandleCenter(int handle, bool* ok) const override;

    // 样式撤销快照：多边形覆写后只记录样式与位置，不写 ShapeType。
    virtual ShapeItemState State() const;
    virtual void ApplyState(const ShapeItemState& state);
    static QPainterPath PathFor(ShapeType type, const QRectF& rect);
    static QPainterPath PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius);
    static QPainterPath PathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle);
    // 当前生效的轮廓：自定义图形按内容矩形缩放单位路径，普通图形交给具体类实现。
    virtual QPainterPath CurrentPath(const QRectF& rect) const;
    // 插入 / 状态与 JSON 缺省时的初始内容尺寸；数值只存在于各子类，基类默认 2 格正方形。
    virtual QSizeF DefaultContentSize() const;

  protected:
    // 具体图形的内置轮廓（不含自定义模板与矩形圆角自动态哨兵）。
    virtual QPainterPath ShapePath(const QRectF& rect) const = 0;
    // 子类可覆写的状态 / JSON 增量。
    virtual void WriteState(ShapeItemState& state) const;
    virtual void ReadState(const ShapeItemState& state);
    virtual void WriteJsonExtra(QJsonObject& json) const;
    virtual void ReadJsonExtra(const QJsonObject& json);
    void PaintContent(QPainter* painter) override;
    void ContentResized() override;
    void DrawSelectionOverlay(QPainter* painter) override;
    void FillChanged() override;
    void StrokeChanged() override;
    void ArcAnglesChanged() override;
    qreal FramePadding() const override { return qMax<qreal>(22.0, StrokeWidth() + 6.0); }

  private:
    qreal m_opacity = 1.0;
    // 圆角半径，单位画布 px；0 表示直角。
    qreal m_cornerRadius = 0.0;
    qreal m_bottomAngle1 = 60.0;
    qreal m_bottomAngle2 = 60.0;
    qreal m_parallelogramAngle = 60.0;
    // 上一次同步后的尺寸，用于区分「拖高」与「拖底边」，避免互相覆盖。
    QSizeF m_triangleSyncSize;
    bool m_syncingTriangle = false;
    bool m_angleReadoutVisible = false;
    QString m_customName;
    QPainterPath m_customPath;
};

// 图形工厂（定义在 shapes/ShapeFactory.cpp）：按 ShapeType 建出具体子类。
ShapeItem* CreateShapeItem(ShapeType type);
