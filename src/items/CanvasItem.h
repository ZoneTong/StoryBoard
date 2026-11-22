#pragma once

#include "items/HandleSpec.h"
#include "items/ItemGeometry.h"
#include "items/properties/ItemProperties.h"
#include "items/properties/TextAttributes.h"

#include <QColor>
#include <QCursor>
#include <QFont>
#include <QGraphicsObject>
#include <QJsonObject>
#include <QMap>
#include <QSizeF>
#include <QString>

class CanvasScene;
class CanvasItemHandles;
class CanvasItemInteraction;
class CanvasItemVisitor;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

// 图元大类：用于按类别过滤（工具栏可用性、属性面板分组等），
// 不靠 Kind() 字符串比较，也不需要上层按叶子类强转。
enum class ItemCategory { SHAPE, LINE, TEXT };

// D16 叶子强转白名单（§2.4）：图元行为优先走 Category / 属性表 / 句柄 / Visitor，
// 只允许下列「无法用标量属性表达」的定点叶子强转，且每处都在源码里注明 D16：
//   1) PolygonItem：合并/分割产物要遍历自己的图层（Layer/Layers），这是多边形独有的数据结构；
//      生产代码统一走 items/polygon/PolygonAccess 的访问者入口（VisitPolygon），不再各自强转。
//   2) TextItem：文字 Memento（TextItemState）只属于文字图元，编辑面板按类别取回带类型的列表；
//      PropertiesPanel::SelectedTexts。
//   3) ArcItem：属性窗中心点行要区分"椭圆中心点"标题（弧线的椭圆框不等于内容框）；
//      ItemPropertiesDialogGeometryBlock::IsEllipseFamily。
// 其余 dynamic_cast 只到基类（ShapeAbstractItem / LineAbstractItem / Fill / Stroke / CanvasScene），
// 属于协议查询而非常量叶子判定，不在预算内。新增叶子强转必须先补进本白名单。
class CanvasItem : public QGraphicsObject {
    Q_OBJECT
  public:
    enum { Type = QGraphicsItem::UserType + 1 };
    // 八向缩放手柄与旋转手柄是公共协议；排在它们之后的是两个通用“自定义句柄槽位”，
    // 具体含义由图元的 CustomHandles() 声明（三角形顶点 C、平行四边形内角旋钮、
    // 扇形 / 弧线的起止端点）。槽位追加在末尾且数值不变，旧调用点不需要迁移。
    enum Handle {
        NO_HANDLE = -1,
        TOP_LEFT = 0,
        TOP,
        TOP_RIGHT,
        RIGHT,
        BOTTOM_RIGHT,
        BOTTOM,
        BOTTOM_LEFT,
        LEFT,
        ROTATION,
        CUSTOM_0,
        CUSTOM_1
    };

    explicit CanvasItem(QGraphicsItem* parent = nullptr);
    ~CanvasItem() override;

    int type() const override { return Type; }

    virtual QString Kind() const = 0;
    virtual QString DisplayName() const = 0;
    // 图元大类由各类根节点给出（图形 / 描线 / 文字），叶子类继承即可。
    virtual ItemCategory Category() const = 0;
    // 跨类型操作的通用入口：具体访问者按 Accept 分派到对应 Visit 方法。
    virtual void Accept(CanvasItemVisitor& visitor);
    // 属性表：各图元只上报自己真正拥有的属性，面板按选中集合求交后逐行显隐。
    // 取值与写回都走同一份标识，界面不再按叶子类强转去读专属字段。
    virtual ItemPropertySet Properties() const;
    virtual QVariant PropertyValue(ItemProperty property) const;
    virtual void SetPropertyValue(ItemProperty property, const QVariant& value);
    // 多属性原子提交：默认逐项委托给 SetPropertyValue，子类需要保证组合一致时可整体覆写。
    virtual void SetPropertyValues(const QMap<ItemProperty, QVariant>& values);
    // 自定义句柄协议：默认没有额外句柄，需要时由叶子给出位置与行为策略。
    virtual HandleSpecList CustomHandles() const;
    // 自定义句柄的拖拽语义由图元实现（句柄命中 / 绘制 / 光标已由 HandleSpec 决定）：
    // 返回 false 表示该句柄没有专用拖拽行为，交给通用缩放路径。
    virtual bool BeginCustomHandleDrag(int handle, const QPointF& scenePos);
    virtual bool ApplyCustomHandleDrag(int handle, const QPointF& scenePos);
    // 一次拖拽（含通用外接框缩放）结束时的回调：收起拖动中的角度读数等临时状态。
    virtual void EndHandleDrag();
    // 拖外接框 / 边缩放期间的逐帧回调：三角形与菱形在这里点亮派生角度读数。
    virtual void FrameResizeDragged();
    // 用于自动命名的类型名：文字/图片不取内容摘要或文件名，而是稳定的类型称呼。
    virtual QString TypeDisplayName() const { return DisplayName(); }
    virtual QJsonObject ToJson() const = 0;
    virtual void FromJson(const QJsonObject& json) = 0;

    // 需求 30：每个图元都有一个可编辑名称。未自定义名称时回退到类型名，
    // 这样属性窗、画布和状态提示里始终能拿到一个人类可读的称呼。
    QString Name() const;
    QString CustomName() const { return m_name; }
    bool HasCustomName() const { return !m_name.isEmpty(); }
    void SetName(const QString& name);

    QRectF ContentRect() const;
    // 选中框与缩放手柄的锚定矩形。默认就是内容矩形；扇形 / 弧线这类
    // 只占内容矩形一部分的图元会覆盖成实际轮廓的最小覆盖矩形。
    virtual QRectF FrameRect() const;
    // §3.4 中心点：派生属性，返回图元绘制中心的场景坐标；写回只平移 pos，
    // 不改尺寸与旋转。椭圆族由叶子覆写成真实椭圆圆心，其余取内容矩形中心。
    virtual QPointF CenterScenePoint() const;
    QSizeF ContentSize() const { return m_size; }
    // 外部请求的尺寸先经过图元自己的规则折算（例如三角形按角度派生高度），
    // 由 setContentSize 一次性应用，避免在一次拖拽里反复改动几何。
    virtual QSizeF AdjustedContentSize(const QSizeF& size) const { return size; }
    void SetContentSize(const QSizeF& size);
    QRectF SceneContentRect() const;

    // 所有图元共用“内容透明度”：只压 PaintContent() 画出的填充、描线、
    // 文字与图片，选中框、手柄等编辑态覆盖层始终按 100% 绘制。
    qreal ItemOpacity() const { return m_itemOpacity; }
    void SetItemOpacity(qreal opacity);

    int LayerId() const { return m_layerId; }
    void SetLayerId(int layerId);
    // 同一分组内的图元在编辑模式下整组移动；0 表示未分组。
    int GroupId() const { return m_groupId; }
    void SetGroupId(int groupId) { m_groupId = qMax(0, groupId); }
    int ZOrder() const { return m_zOrder; }
    void SetZOrder(int zOrder);
    // 需求 24：图元可以绕内容中心旋转；旋转角随画布 JSON 保存并参与撤销。
    qreal RotationAngle() const { return m_rotation; }
    void SetRotationAngle(qreal angle);
    // 旋转手柄与快捷键使用：角度落在 [-180, 180)。
    static qreal NormalizeRotation(qreal angle);
    static qreal SnapRotation(qreal angle, qreal step = 5.0);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    // 图元的可见轮廓（不含选中控制点），非查看模式下用于提示被遮盖的图元。
    virtual QPainterPath OutlineShape() const;
    // 图元真正涂满画布、能够遮挡下方图元的区域：无填充图形只算描边，
    // 描边类图元只算笔迹本身。既用于判断自己被遮住多少，也用于遮挡别人。
    virtual QPainterPath PaintedShape() const;
    // 绘制时从下层图元可见区域中减去的形状。默认与 PaintedShape() 相同；
    // 文字只画字形，不能按字形裁剪下层，否则低缩放时字边会露出背景形成重影。
    virtual QPainterPath ClippingShape() const;

    bool IsEditable() const;
    bool IsExporting() const;

    // 旋转手柄的屏幕尺寸随视图缩放变化；缩放后重新声明 boundingRect。
    void RefreshViewDependentGeometry();

    static QSizeF MinimumSize();

  protected:
    virtual void PaintContent(QPainter* painter) = 0;
    virtual void ContentResized() {}
    virtual qreal FramePadding() const { return 42.0; }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    QJsonObject BaseJson() const;
    void ApplyBaseJson(const QJsonObject& json);
    CanvasScene* canvasScene() const;
    qreal ViewScale() const;
    bool HasSingleSelection() const;
    qreal HandleSize() const;

    QPointF HandleCenter(Handle handle) const;
    QRectF HandleRect(Handle handle) const;
    Handle HandleAt(const QPointF& pos) const;
    qreal RotationHandleOffset() const;
    // Clickable area of the resize handles drawn by DrawSelectionOverlay().
    // Handles are centred on the content corners, so half of every handle sits
    // outside shape(); item hit tests must include it explicitly.
    QPainterPath SelectionHandleShape() const;
    virtual void DrawSelectionOverlay(QPainter* painter);
    QCursor CursorForHandle(Handle handle) const;

  private:
    friend class CanvasItemHandles;
    friend class CanvasItemInteraction;

    enum class DragKind { NONE, MOVE, RESIZE, ROTATE };

    QSizeF m_size{120.0, 90.0};
    int m_layerId = 0;
    int m_groupId = 0;
    int m_zOrder = -1;
    qreal m_rotation = 0.0;
    qreal m_itemOpacity = 1.0;
    QString m_name;
    DragKind m_drag = DragKind::NONE;
    Handle m_resizeHandle = NO_HANDLE;
    QPointF m_dragStartScene;
    QPointF m_dragStartPos;
    QSizeF m_dragStartSize;
    qreal m_dragStartRotation = 0.0;
    qreal m_rotateStartAngle = 0.0;
    ItemGeometryList m_before;
    CanvasItemHandles* m_handles = nullptr;
    CanvasItemInteraction* m_interaction = nullptr;
};
