#pragma once

#include "ItemGeometry.h"
#include "ItemProperties.h"
#include "TextAttributes.h"

#include <QColor>
#include <QCursor>
#include <QFont>
#include <QGraphicsObject>
#include <QJsonObject>
#include <QSizeF>
#include <QString>

class CanvasScene;
class CanvasItemHandles;
class CanvasItemInteraction;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

class CanvasItem : public QGraphicsObject {
    Q_OBJECT
  public:
    enum { Type = QGraphicsItem::UserType + 1 };
    // 需求 2：Apex 是三角形顶点 C 的专用句柄，数值排在 Rotation 之后，旧值不受影响。
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
        APEX,
        // 倾斜旋钮：平行四边形上边中点的内角手柄，水平拖动连续改变内角（锐角↔钝角）。
        // 追加在末尾，避免既有句柄值发生位移。
        SKEW
    };

    explicit CanvasItem(QGraphicsItem* parent = nullptr);
    ~CanvasItem() override;

    int type() const override { return Type; }

    virtual QString Kind() const = 0;
    virtual QString DisplayName() const = 0;
    // 属性表：各图元只上报自己真正拥有的属性，面板按选中集合求交后逐行显隐。
    // 取值与写回都走同一份标识，界面不再按叶子类强转去读专属字段。
    virtual ItemPropertySet Properties() const;
    virtual QVariant PropertyValue(ItemProperty property) const;
    virtual void SetPropertyValue(ItemProperty property, const QVariant& value);
    // 用于自动命名的类型名：文字/图片不取内容摘要或文件名，而是稳定的类型称呼。
    virtual QString TypeDisplayName() const { return DisplayName(); }
    // 需求 2：非矩形句柄（三角形顶点 C）由图元给出本地坐标；默认不支持。
    virtual QPointF CustomHandleCenter(int handle, bool* ok) const;
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
    QSizeF ContentSize() const { return m_size; }
    // 外部请求的尺寸先经过图元自己的规则折算（例如三角形按角度派生高度），
    // 由 setContentSize 一次性应用，避免在一次拖拽里反复改动几何。
    virtual QSizeF AdjustedContentSize(const QSizeF& size) const { return size; }
    void SetContentSize(const QSizeF& size);
    QRectF SceneContentRect() const;

    // 透明度是所有图元共有的属性；图形因为有自绘填充，单独覆盖这两个接口。
    // 文字能力查询：非文字图元返回 false / 空值，文字图元由 TextItem 覆写。
    virtual bool HasTextFacet() const { return false; }
    virtual QString TextContent() const { return QString(); }
    virtual QFont TextFont() const { return QFont(); }
    virtual QColor TextColor() const { return QColor(); }
    virtual QSizeF TextNaturalSize() const { return QSizeF(); }
    virtual TextItemState TextState() const { return TextItemState(); }
    virtual void ApplyTextState(const TextItemState& state) { Q_UNUSED(state) }

    virtual qreal ItemOpacity() const { return QGraphicsItem::opacity(); }
    virtual void SetItemOpacity(qreal opacity);

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
