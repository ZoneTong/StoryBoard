#pragma once

#include "BoardItem.h"

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPainterPath>

enum class ShapeType {
    Rectangle = 0,
    RoundedRectangle,
    Ellipse,
    Triangle,
    Diamond,
    Star,
    Arrow,
    DoubleArrow,
    Line,
    BraceLeft,
    BraceRight,
    Callout
};

QList<ShapeType> allShapeTypes();
QString shapeTypeName(ShapeType type);
QString shapeTypeToString(ShapeType type);
ShapeType shapeTypeFromString(const QString& value);
QSizeF defaultShapeSize(ShapeType type);
bool shapeTypeIsFilled(ShapeType type);
bool shapeTypeIsStrokeOnly(ShapeType type);
QIcon makeShapeIcon(ShapeType type, const QColor& color = QColor(58, 64, 80),
                    int logicalSize = 22);

struct ShapeItemState {
    ShapeType type = ShapeType::Rectangle;
    QColor strokeColor = QColor(30, 41, 59);
    QColor fillColor = QColor(30, 41, 59, 36);
    qreal strokeWidth = 4.0;
    QPointF pos;
    QSizeF size;

    bool operator==(const ShapeItemState& other) const
    {
        return type == other.type
            && strokeColor == other.strokeColor
            && fillColor == other.fillColor
            && qFuzzyCompare(strokeWidth, other.strokeWidth)
            && pos == other.pos
            && size == other.size;
    }

    bool operator!=(const ShapeItemState& other) const { return !(*this == other); }
};

class ShapeItem : public BoardItem
{
    Q_OBJECT
public:
    explicit ShapeItem(ShapeType type = ShapeType::Rectangle,
                       QGraphicsItem* parent = nullptr);

    QString kind() const override { return QStringLiteral("shape"); }
    QString displayName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    QPainterPath shape() const override;

    ShapeType shapeType() const { return m_type; }
    void setShapeType(ShapeType type);
    QColor strokeColor() const { return m_strokeColor; }
    QColor fillColor() const { return m_fillColor; }
    qreal strokeWidth() const { return m_strokeWidth; }
    void setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);

    ShapeItemState state() const;
    void applyState(const ShapeItemState& state);

    static QPainterPath pathFor(ShapeType type, const QRectF& rect);

protected:
    void paintContent(QPainter* painter) override;
    qreal framePadding() const override { return qMax<qreal>(22.0, m_strokeWidth + 6.0); }

private:
    ShapeType m_type = ShapeType::Rectangle;
    QColor m_strokeColor = QColor(30, 41, 59);
    QColor m_fillColor = QColor(30, 41, 59, 36);
    qreal m_strokeWidth = 4.0;
};
