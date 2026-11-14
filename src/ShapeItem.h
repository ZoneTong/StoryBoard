#pragma once

#include "CanvasItem.h"

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
    Callout,
    Sector,
    Arc
};

enum class FillStyle { None = 0, Solid, Hachure, ReverseHachure, CrossHatch, GridHatch, Honeycomb };

QList<ShapeType> allShapeTypes();
QString shapeTypeName(ShapeType type);
QString shapeTypeToString(ShapeType type);
ShapeType shapeTypeFromString(const QString& value);
QSizeF defaultShapeSize(ShapeType type);
bool shapeTypeIsFilled(ShapeType type);
bool shapeTypeIsStrokeOnly(ShapeType type);
QIcon makeShapeIcon(ShapeType type, const QColor& color = QColor(58, 64, 80), int logicalSize = 22);

QList<FillStyle> allFillStyles();
QString fillStyleName(FillStyle style);
QString fillStyleToString(FillStyle style);
FillStyle fillStyleFromString(const QString& value);
// 填充纹理的中心线几何，编辑图元卡的工具按钮缩略图与图元绘制共用这一份。
QPainterPath fillPatternCenterLines(FillStyle style, const QRectF& rect);

struct ShapeItemState {
    ShapeType type = ShapeType::Rectangle;
    QColor strokeColor = QColor(30, 41, 59);
    QColor fillColor = QColor(30, 41, 59, 36);
    FillStyle fillStyle = FillStyle::Solid;
    qreal strokeWidth = 4.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    qreal opacity = 1.0;
    qreal cornerRadius = -1.0;
    // 扇形 / 弧线：起始角与扫过角（度，Qt 坐标：0° 在三点钟方向，逆时针为正）。
    qreal startAngle = 30.0;
    qreal sweepAngle = 120.0;
    // 扇形 / 弧线的椭圆半径（像素，横向）；< 0 表示按内容矩形宽度的一半自动取值。
    qreal shapeRadius = -1.0;
    QPointF pos;
    QSizeF size;

    bool operator==(const ShapeItemState& other) const
    {
        return type == other.type && strokeColor == other.strokeColor && fillColor == other.fillColor &&
               fillStyle == other.fillStyle && qFuzzyCompare(strokeWidth, other.strokeWidth) &&
               strokeStyle == other.strokeStyle && qFuzzyCompare(opacity, other.opacity) &&
               qFuzzyCompare(cornerRadius + 2.0, other.cornerRadius + 2.0) &&
               qFuzzyCompare(startAngle + 2.0, other.startAngle + 2.0) &&
               qFuzzyCompare(sweepAngle + 2.0, other.sweepAngle + 2.0) &&
               qFuzzyCompare(shapeRadius + 2.0, other.shapeRadius + 2.0) && pos == other.pos && size == other.size;
    }

    bool operator!=(const ShapeItemState& other) const { return !(*this == other); }
};

class ShapeItem : public CanvasItem {
    Q_OBJECT
  public:
    explicit ShapeItem(ShapeType type = ShapeType::Rectangle, QGraphicsItem* parent = nullptr);

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
    Qt::PenStyle strokeStyle() const { return m_strokeStyle; }
    qreal opacity() const { return m_opacity; }
    qreal cornerRadius() const { return m_cornerRadius; }
    qreal startAngle() const { return m_startAngle; }
    qreal sweepAngle() const { return m_sweepAngle; }
    qreal shapeRadius() const { return m_shapeRadius; }
    void setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth);
    void setFill(const QColor& fillColor, FillStyle style);
    void setFillStyle(FillStyle style);
    void setStrokeStyle(Qt::PenStyle style);
    void setOpacity(qreal opacity);
    void setCornerRadius(qreal radius);
    void setArcAngles(qreal startAngle, qreal sweepAngle);
    void setShapeRadius(qreal radius);

    // 自定义图形：轮廓来自画布模板，存的是 0–1 单位坐标；没有自定义轮廓时按 shapeType 画。
    bool isCustomShape() const { return !m_customPath.isEmpty(); }
    QString customShapeName() const { return m_customName; }
    QPainterPath customShapePath() const { return m_customPath; }
    void setCustomShape(const QString& name, const QPainterPath& unitPath);

    ShapeItemState state() const;
    void applyState(const ShapeItemState& state);

    static QPainterPath pathFor(ShapeType type, const QRectF& rect);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius);
    static QPainterPath pathFor(ShapeType type, const QRectF& rect, qreal cornerRadius, qreal startAngle,
                                qreal sweepAngle, qreal shapeRadius);
    // 当前生效的轮廓：自定义图形按内容矩形缩放单位路径，普通图形带圆角参数。
    QPainterPath currentPath(const QRectF& rect) const;

  protected:
    void paintContent(QPainter* painter) override;
    qreal framePadding() const override { return qMax<qreal>(22.0, m_strokeWidth + 6.0); }

  private:
    ShapeType m_type = ShapeType::Rectangle;
    QColor m_strokeColor = QColor(30, 41, 59);
    QColor m_fillColor = QColor(30, 41, 59, 36);
    FillStyle m_fillStyle = FillStyle::Solid;
    qreal m_strokeWidth = 4.0;
    Qt::PenStyle m_strokeStyle = Qt::SolidLine;
    qreal m_opacity = 1.0;
    // < 0 表示用该图形类型的默认圆角（只有矩形 / 圆角矩形生效）。
    qreal m_cornerRadius = -1.0;
    qreal m_startAngle = 30.0;
    qreal m_sweepAngle = 120.0;
    qreal m_shapeRadius = -1.0;
    QString m_customName;
    QPainterPath m_customPath;
};