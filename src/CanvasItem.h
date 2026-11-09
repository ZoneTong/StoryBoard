#pragma once

#include <QCursor>
#include <QGraphicsObject>
#include <QJsonObject>
#include <QSizeF>

#include "ItemGeometry.h"

class CanvasScene;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

class CanvasItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum { Type = QGraphicsItem::UserType + 1 };
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
        Rotation
    };

    explicit CanvasItem(QGraphicsItem* parent = nullptr);

    int type() const override { return Type; }

    virtual QString kind() const = 0;
    virtual QString displayName() const = 0;
    // 用于自动命名的类型名：文字/图片不取内容摘要或文件名，而是稳定的类型称呼。
    virtual QString typeDisplayName() const { return displayName(); }
    virtual QJsonObject toJson() const = 0;
    virtual void fromJson(const QJsonObject& json) = 0;

    // 需求 30：每个图元都有一个可编辑名称。未自定义名称时回退到类型名，
    // 这样属性窗、画布和状态提示里始终能拿到一个人类可读的称呼。
    QString name() const;
    QString rawName() const { return m_name; }
    void setName(const QString& name);

    QRectF contentRect() const;
    QSizeF contentSize() const { return m_size; }
    void setContentSize(const QSizeF& size);
    QRectF sceneContentRect() const;

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
    void drawSelectionOverlay(QPainter* painter);
    QCursor cursorForHandle(Handle handle) const;

private:
    enum class DragKind { None, Move, Resize, Rotate };

    void beginMove(const QPointF& scenePos);
    void beginResize(Handle handle, const QPointF& scenePos);
    void beginRotate(const QPointF& scenePos);
    void applyMove(const QPointF& scenePos);
    void applyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers);
    void applyRotate(const QPointF& scenePos);
    void finishInteraction();
    void updateHoverCursor(const QPointF& pos);

    QSizeF m_size { 120.0, 90.0 };
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
};
