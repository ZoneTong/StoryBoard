#pragma once

#include <QCursor>
#include <QGraphicsObject>
#include <QJsonObject>
#include <QSizeF>

#include "ItemGeometry.h"

class BoardScene;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

class BoardItem : public QGraphicsObject
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
        Left
    };

    explicit BoardItem(QGraphicsItem* parent = nullptr);

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
    int height() const { return m_height; }
    void setHeight(int height);

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

    static QSizeF minimumSize();

protected:
    virtual void paintContent(QPainter* painter) = 0;
    virtual void contentResized() {}
    virtual qreal framePadding() const { return 26.0; }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    QJsonObject baseJson() const;
    void applyBaseJson(const QJsonObject& json);
    BoardScene* boardScene() const;
    qreal viewScale() const;
    bool hasSingleSelection() const;
    qreal handleSize() const;

    QPointF handleCenter(Handle handle) const;
    QRectF handleRect(Handle handle) const;
    Handle handleAt(const QPointF& pos) const;
    // Clickable area of the resize handles drawn by drawSelectionOverlay().
    // Handles are centred on the content corners, so half of every handle sits
    // outside shape(); item hit tests must include it explicitly.
    QPainterPath selectionHandleShape() const;
    void drawSelectionOverlay(QPainter* painter);
    QCursor cursorForHandle(Handle handle) const;

private:
    enum class DragKind { None, Move, Resize };

    void beginMove(const QPointF& scenePos);
    void beginResize(Handle handle, const QPointF& scenePos);
    void applyMove(const QPointF& scenePos);
    void applyResize(const QPointF& scenePos, Qt::KeyboardModifiers modifiers);
    void finishInteraction();
    void updateHoverCursor(const QPointF& pos);

    QSizeF m_size { 120.0, 90.0 };
    int m_layerId = 0;
    int m_height = -1;
    QString m_name;
    DragKind m_drag = DragKind::None;
    Handle m_resizeHandle = NoHandle;
    QPointF m_dragStartScene;
    QPointF m_dragStartPos;
    QSizeF m_dragStartSize;
    ItemGeometryList m_before;
};
