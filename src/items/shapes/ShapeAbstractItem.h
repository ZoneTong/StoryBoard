#pragma once

#include "items/CanvasItem.h"
#include "items/properties/ArrowStyle.h"
#include "items/properties/FillAttributes.h"
#include "items/properties/ImageFill.h"
#include "items/properties/StrokeAttributes.h"

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

struct ShapeItemState {
    // 填充 / 描线两组属性直接复用属性基类的快照，不再在图形状态里重复字段。
    FillAttributesState fill;
    StrokeAttributesState stroke;
    qreal opacity = 1.0;
    QPointF pos;
    QSizeF size;
    // 类型专属标量（圆角 / 三角形底角 / 平行四边形内角等）一律放这里，
    // 按 ItemProperty 键回放；公共快照不再为每个叶子加扁平字段。
    QMap<ItemProperty, QVariant> propertyExtras;

    bool operator==(const ShapeItemState& other) const
    {
        return fill == other.fill && stroke == other.stroke && qFuzzyCompare(opacity, other.opacity) &&
               pos == other.pos && size == other.size && propertyExtras == other.propertyExtras;
    }

    bool operator!=(const ShapeItemState& other) const { return !(*this == other); }
};

class ShapeAbstractItem : public CanvasItem, public FillAttributes, public StrokeAttributes {
    Q_OBJECT
  public:
    // 需求 2：带短刻度线的底边是 AB（A 在左、B 在右），对面顶点是 C。
    enum class TriangleVertex { A, B, C };
    explicit ShapeAbstractItem(QGraphicsItem* parent = nullptr);

    QString Kind() const override { return QStringLiteral("shape"); }
    ItemCategory Category() const override { return ItemCategory::SHAPE; }
    void Accept(CanvasItemVisitor& visitor) override;
    // 每种图形由 src/shapes/ 下的具体类给出身份；基类不再保存 ShapeType。
    virtual ShapeType shapeType() const = 0;
    // 属性表：填充 / 描线六件套为图形族公共项，各子类再按需要追加。
    ItemPropertySet Properties() const override;
    QVariant PropertyValue(ItemProperty property) const override;
    void SetPropertyValue(ItemProperty property, const QVariant& value) override;
    // 多边形的内部图层不再向基类暴露只读视图：需要按层取几何的调用方
    // 直接使用 PolygonItem::Layers()（dynamic_cast 到 PolygonItem 属白名单定点场景）。
    QString DisplayName() const override;
    QJsonObject ToJson() const override;
    void FromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;
    QPainterPath OutlineShape() const override;
    QPainterPath PaintedShape() const override;
    QRectF FrameRect() const override;

    // 填充 / 描线的读写接口由两个属性基类提供；多边形按内部图层覆写其中若干条。
    // 透明度已收口到 CanvasItem，按“内容透明度”统一处理。
    qreal CornerRadius() const { return m_cornerRadius; }
    qreal BottomAngle1() const { return m_bottomAngle1; }
    qreal BottomAngle2() const { return m_bottomAngle2; }
    void SetAngleReadoutVisible(bool visible);
    bool AngleReadoutVisible() const { return m_angleReadoutVisible; }
    virtual void SetStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);
    void SetCornerRadius(qreal radius);
    // 三角形：设置底边两侧的底角（度）。角度优先，底边保持不变（面板与撤销用）；
    // 两角和被钳制在 175° 以内，保证顶点不会跑出内容矩形。
    void SetTriangleAngles(qreal angle1, qreal angle2);
    // 拖顶点专用（§3.1）：AB 边上的垂直高度保持不变，只有角度与包围框宽度变。
    void SetTriangleAnglesFixedHeight(qreal angle1, qreal angle2, qreal height);
    // 自定义图形：轮廓来自画布模板，存的是 0–1 单位坐标；没有自定义轮廓时按子类轮廓画。
    bool IsCustomShape() const { return !m_customPath.isEmpty(); }
    QString CustomShapeName() const { return m_customName; }
    QPainterPath CustomShapePath() const { return m_customPath; }
    void SetCustomShape(const QString& name, const QPainterPath& unitPath);

    // 三角形三个顶点（内容矩形局部坐标）：A/B 为底边两端，C 为顶点。
    QPointF triangleVertex(TriangleVertex vertex) const;
    // P2 自定义句柄拖拽：三角形顶点 C 的内角手柄（位置由 TriangleItem 的 CustomHandles 给出）。
    bool BeginCustomHandleDrag(int handle, const QPointF& scenePos) override;
    bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos) override;
    void EndHandleDrag() override;
    void FrameResizeDragged() override;

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
    qreal FramePadding() const override { return qMax<qreal>(22.0, StrokeWidth() + 6.0); }

  private:
    // 圆角半径，单位画布 px；0 表示直角。
    qreal m_cornerRadius = 0.0;
    qreal m_bottomAngle1 = 60.0;
    qreal m_bottomAngle2 = 60.0;
    // 角度 / 状态回放期间的尺寸同步屏蔽：避免 SetContentSize 触发的
    // ContentResized 反算 B 覆盖刚写入的角度。
    bool m_syncingTriangle = false;
    bool m_angleReadoutVisible = false;
    // 拖顶点 C 期间固定的底边锚点（场景坐标）与 AB 边上的垂直高度。
    QPointF m_apexDragBaseA;
    QPointF m_apexDragBaseB;
    qreal m_apexDragHeight = 1.0;
    QString m_customName;
    QPainterPath m_customPath;
};

// 图形工厂（定义在 shapes/ShapeFactory.cpp）：按 ShapeType 建出具体子类。
ShapeAbstractItem* CreateShapeItem(ShapeType type);
