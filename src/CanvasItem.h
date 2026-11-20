#pragma once

#include "ItemGeometry.h"
#include "ItemProperties.h"

#include <QColor>
#include <QCursor>
#include <QFont>
#include <QGraphicsObject>
#include <QJsonObject>
#include <QSizeF>
#include <QString>

// 文字图元的状态快照（撤销 / 属性窗共用）；放在基类头里，任何图元都能持有，
// 上层不必为了读写文字而强转到 TextItem。
struct TextItemState {
    QString text;
    QFont font;
    QColor color;
    QPointF pos;
    QSizeF size;
    QSizeF baseSize;

    bool operator==(const TextItemState& other) const
    {
        return text == other.text && font == other.font && color == other.color && pos == other.pos &&
               size == other.size && baseSize == other.baseSize;
    }

    bool operator!=(const TextItemState& other) const { return !(*this == other); }
};

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
        NoHandle = -1,
        TopLeft = 0,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        Rotation,
        Apex
    };

    explicit CanvasItem(QGraphicsItem* parent = nullptr);
    ~CanvasItem() override;

    int type() const override { return Type; }

    virtual QString kind() const = 0;
    virtual QString displayName() const = 0;
    // 属性表：各图元只上报自己真正拥有的属性，面板按选中集合求交后逐行显隐。
    // 取值与写回都走同一份标识，界面不再按叶子类强转去读专属字段。
    virtual ItemPropertySet properties() const;
    virtual QVariant propertyValue(ItemProperty property) const;
    virtual void setPropertyValue(ItemProperty property, const QVariant& value);
    // 用于自动命名的类型名：文字/图片不取内容摘要或文件名，而是稳定的类型称呼。
    virtual QString typeDisplayName() const { return displayName(); }
    // 需求 2：非矩形句柄（三角形顶点 C）由图元给出本地坐标；默认不支持。
    virtual QPointF customHandleCenter(int handle, bool* ok) const;
    virtual QJsonObject toJson() const = 0;
    virtual void fromJson(const QJsonObject& json) = 0;

    // 需求 30：每个图元都有一个可编辑名称。未自定义名称时回退到类型名，
    // 这样属性窗、画布和状态提示里始终能拿到一个人类可读的称呼。
    QString name() const;
    QString rawName() const { return m_name; }
    void setName(const QString& name);

    QRectF contentRect() const;
    // 选中框与缩放手柄的锚定矩形。默认就是内容矩形；扇形 / 弧线这类
    // 只占内容矩形一部分的图元会覆盖成实际轮廓的最小覆盖矩形。
    virtual QRectF frameRect() const;
    QSizeF contentSize() const { return m_size; }
    // 外部请求的尺寸先经过图元自己的规则折算（例如三角形按角度派生高度），
    // 由 setContentSize 一次性应用，避免在一次拖拽里反复改动几何。
    virtual QSizeF adjustedContentSize(const QSizeF& size) const { return size; }
    void setContentSize(const QSizeF& size);
    QRectF sceneContentRect() const;

    // 透明度是所有图元共有的属性；图形因为有自绘填充，单独覆盖这两个接口。
    // 文字能力查询：非文字图元返回 false / 空值，文字图元由 TextItem 覆写。
    virtual bool hasTextFacet() const { return false; }
    virtual QString textContent() const { return QString(); }
    virtual QFont textFont() const { return QFont(); }
    virtual QColor textColor() const { return QColor(); }
    virtual QSizeF textNaturalSize() const { return QSizeF(); }
    virtual TextItemState textState() const { return TextItemState(); }
    virtual void applyTextState(const TextItemState& state) { Q_UNUSED(state) }

    virtual qreal itemOpacity() const { return QGraphicsItem::opacity(); }
    virtual void setItemOpacity(qreal opacity);

    int layerId() const { return m_layerId; }
    void setLayerId(int layerId);
    // 同一分组内的图元在编辑模式下整组移动；0 表示未分组。
    int groupId() const { return m_groupId; }
    void setGroupId(int groupId) { m_groupId = qMax(0, groupId); }
    int height() const { return m_height; }
    void setHeight(int height);
    // 需求 24：图元可以绕内容中心旋转；旋转角随画布 JSON 保存并参与撤销。
    qreal rotationAngle() const { return m_rotation; }
    void setRotationAngle(qreal angle);
    // 旋转手柄与快捷键使用：角度落在 [-180, 180)。
    static qreal normalizeRotation(qreal angle);
    static qreal snapRotation(qreal angle, qreal step = 5.0);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    // 图元的可见轮廓（不含选中控制点），非查看模式下用于提示被遮盖的图元。
    virtual QPainterPath outlineShape() const;
    // 图元真正涂满画布、能够遮挡下方图元的区域：无填充图形只算描边，
    // 描边类图元只算笔迹本身。既用于判断自己被遮住多少，也用于遮挡别人。
    virtual QPainterPath paintedShape() const;
    // 绘制时从下层图元可见区域中减去的形状。默认与 paintedShape() 相同；
    // 文字只画字形，不能按字形裁剪下层，否则低缩放时字边会露出背景形成重影。
    virtual QPainterPath clippingShape() const;

    bool isEditable() const;
    bool isExporting() const;

    // 旋转手柄的屏幕尺寸随视图缩放变化；缩放后重新声明 boundingRect。
    void refreshViewDependentGeometry();

    static QSizeF minimumSize();

  protected:
    virtual void paintContent(QPainter* painter) = 0;
    virtual void contentResized() {}
    virtual qreal framePadding() const { return 42.0; }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    QJsonObject baseJson() const;
    void applyBaseJson(const QJsonObject& json);
    CanvasScene* canvasScene() const;
    qreal viewScale() const;
    bool hasSingleSelection() const;
    qreal handleSize() const;

    QPointF handleCenter(Handle handle) const;
    QRectF handleRect(Handle handle) const;
    Handle handleAt(const QPointF& pos) const;
    qreal rotationHandleOffset() const;
    // Clickable area of the resize handles drawn by drawSelectionOverlay().
    // Handles are centred on the content corners, so half of every handle sits
    // outside shape(); item hit tests must include it explicitly.
    QPainterPath selectionHandleShape() const;
    virtual void drawSelectionOverlay(QPainter* painter);
    QCursor cursorForHandle(Handle handle) const;

  private:
    friend class CanvasItemHandles;
    friend class CanvasItemInteraction;

    enum class DragKind { None, Move, Resize, Rotate };

    QSizeF m_size{120.0, 90.0};
    int m_layerId = 0;
    int m_groupId = 0;
    int m_height = -1;
    qreal m_rotation = 0.0;
    QString m_name;
    DragKind m_drag = DragKind::None;
    Handle m_resizeHandle = NoHandle;
    QPointF m_dragStartScene;
    QPointF m_dragStartPos;
    QSizeF m_dragStartSize;
    qreal m_dragStartRotation = 0.0;
    qreal m_rotateStartAngle = 0.0;
    ItemGeometryList m_before;
    CanvasItemHandles* m_handles = nullptr;
    CanvasItemInteraction* m_interaction = nullptr;
};
