#pragma once

#include <QColor>
#include <QIcon>
#include <QList>

#include "BoardItem.h"

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

enum class FillStyle {
    None = 0,
    Solid,
    Hachure,
    ReverseHachure,
    CrossHatch,
    GridHatch,
    Honeycomb
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

QList<FillStyle> allFillStyles();
QString fillStyleName(FillStyle style);
QString fillStyleToString(FillStyle style);
FillStyle fillStyleFromString(const QString& value);

struct ShapeItemState {
    ShapeType type = ShapeType::Rectangle;
    QColor strokeColor = QColor(30, 41, 59);
    QColor fillColor = QColor(30, 41, 59, 36);
    FillStyle fillStyle = FillStyle::Solid;
    qreal strokeWidth = 4.0;
    QPointF pos;
    QSizeF size;

    bool operator==(const ShapeItemState& other) const
    {
        return type == other.type
            && strokeColor == other.strokeColor
            && fillColor == other.fillColor
            && fillStyle == other.fillStyle
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
    QPainterPath outlineShape() const override;
    QPainterPath paintedShape() const override;

    ShapeType shapeType() const { return m_type; }
    void setShapeType(ShapeType type);
    QColor strokeColor() const { return m_strokeColor; }
    QColor fillColor() const { return m_fillColor; }
    FillStyle fillStyle() const { return m_fillStyle; }
    qreal strokeWidth() const { return m_strokeWidth; }
    void setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);
    void setFill(const QColor& fillColor, FillStyle style);
    void setFillStyle(FillStyle style);

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
    FillStyle m_fillStyle = FillStyle::Solid;
    qreal m_strokeWidth = 4.0;
};