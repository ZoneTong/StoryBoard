#include "ShapeAbstractItem.h"
#include "ShapeMeta.h"
#include "ShapePainter.h"
#include "ShapePathBuilder.h"
#include "shapes/ShapeFactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

// 填充纹理的中心线。绘制与遮挡计算共用这一份几何，保证看到的和“挡住别人的”一致。
QPainterPath FillPatternCenterLines(FillStyle style, const QRectF& rect)
{
    return ShapePainter::CenterLines(style, rect);
}

QList<ShapeType> AllShapeTypes()
{
    return ShapeMeta::AllTypes();
}

QString ShapeTypeName(ShapeType type)
{
    switch (type) {
    case ShapeType::RECTANGLE:
        return ShapeAbstractItem::tr("Rectangle");
    case ShapeType::ELLIPSE:
        return ShapeAbstractItem::tr("Ellipse");
    case ShapeType::TRIANGLE:
        return ShapeAbstractItem::tr("Triangle");
    case ShapeType::STAR:
        return ShapeAbstractItem::tr("Star");
    case ShapeType::ARROW:
        return ShapeAbstractItem::tr("Arrow");
    case ShapeType::DOUBLE_ARROW:
        return ShapeAbstractItem::tr("Double arrow");

    case ShapeType::CALLOUT:
        return ShapeAbstractItem::tr("Speech bubble");
    case ShapeType::SECTOR:
        return ShapeAbstractItem::tr("Sector");

    case ShapeType::STAR4:
        return ShapeAbstractItem::tr("Four-point star");
    case ShapeType::HEXAGRAM:
        return ShapeAbstractItem::tr("Six-point star");
    case ShapeType::HEART:
        return ShapeAbstractItem::tr("Heart");

    case ShapeType::FLAG_WAVE:
        return ShapeAbstractItem::tr("Wavy flag");
    case ShapeType::SCROLL:
        return ShapeAbstractItem::tr("Scroll");
    case ShapeType::CUBE:
        return ShapeAbstractItem::tr("Cube");
    case ShapeType::CYLINDER:
        return ShapeAbstractItem::tr("Cylinder");
    case ShapeType::PARALLELOGRAM:
        return ShapeAbstractItem::tr("Parallelogram");
    case ShapeType::DIAMOND:
        return ShapeAbstractItem::tr("Diamond");
    }
    return ShapeAbstractItem::tr("Rectangle");
}

QString ShapeTypeToString(ShapeType type)
{
    return ShapeMeta::TypeToString(type);
}

ShapeType ShapeTypeFromString(const QString& value)
{
    return ShapeMeta::TypeFromString(value);
}

QSizeF DefaultShapeSize(ShapeType type)
{
    // 单一来源在子类 DefaultContentSize()：按需构造原型图元取默认尺寸，
    // 构造函数走 SetContentSize(DefaultContentSize())，不会递归。
    ShapeAbstractItem* prototype = CreateShapeItem(type);
    const QSizeF size = prototype->DefaultContentSize();
    delete prototype;
    return size;
}

bool ShapeTypeIsFilled(ShapeType type)
{
    return ShapeMeta::IsFilled(type);
}


QList<FillStyle> AllFillStyles()
{
    return ShapeMeta::AllStyles();
}

QString FillStyleName(FillStyle style)
{
    switch (style) {
    case FillStyle::NONE:
        return ShapeAbstractItem::tr("No fill");
    case FillStyle::SOLID:
        return ShapeAbstractItem::tr("Solid");
    case FillStyle::HACHURE:
        return ShapeAbstractItem::tr("Hatch");
    case FillStyle::REVERSE_HACHURE:
        return ShapeAbstractItem::tr("Reverse hatch");
    case FillStyle::CROSS_HATCH:
        return ShapeAbstractItem::tr("Cross hatch");
    case FillStyle::GRID_HATCH:
        return ShapeAbstractItem::tr("Grid hatch");
    case FillStyle::HONEYCOMB:
        return ShapeAbstractItem::tr("Hexagon");
    case FillStyle::HORIZONTAL_HATCH:
        return ShapeAbstractItem::tr("Horizontal lines");
    case FillStyle::VERTICAL_HATCH:
        return ShapeAbstractItem::tr("Vertical lines");
    case FillStyle::BRICK:
        return ShapeAbstractItem::tr("Brick");
    case FillStyle::DOTS:
        return ShapeAbstractItem::tr("Dots");
    }
    return ShapeAbstractItem::tr("Solid");
}

QString FillStyleToString(FillStyle style)
{
    return ShapeMeta::StyleToString(style);
}

FillStyle FillStyleFromString(const QString& value)
{
    return ShapeMeta::StyleFromString(value);
}

QIcon MakeShapeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    return ShapePainter::MakeIcon(type, color, logicalSize);
}

QIcon MakeShapePresetIcon(const QString& preset, const QColor& color, int logicalSize)
{
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QRectF box(3.0, 4.0, 18.0, 16.0);
    if (preset == QStringLiteral("roundedRect")) {
        // 与插入值一致：圆角取短边的 16%。
        const qreal radius = qMin(box.height(), box.width()) * 0.16;
        painter.drawRoundedRect(box, radius, radius);
    } else if (preset == QStringLiteral("triangle45")) {
        // 45° 预置：底角 90° 在左下、45° 在右下，顶点落在左上角，画成等腰直角三角形。
        const QRectF square(5.0, 5.0, 14.0, 14.0);
        QPainterPath path;
        path.moveTo(square.left(), square.top());
        path.lineTo(square.left(), square.bottom());
        path.lineTo(square.right(), square.bottom());
        path.closeSubpath();
        painter.drawPath(path);
    } else if (preset == QStringLiteral("triangle60")) {
        // 60° 预置：同一条竖边，底角 60° 让底边更短，整体更瘦高。
        const qreal height = 15.0;
        const qreal width = height / qTan(qDegreesToRadians(60.0));
        const QRectF tall(6.0, 4.5, width, height);
        QPainterPath path;
        path.moveTo(tall.left(), tall.top());
        path.lineTo(tall.left(), tall.bottom());
        path.lineTo(tall.right(), tall.bottom());
        path.closeSubpath();
        painter.drawPath(path);
    } else if (preset == QStringLiteral("braceLeft") || preset == QStringLiteral("braceRight")) {
        // 需求 13：花括号已是曲线图元，图标直接用它插入时的采样路径绘制。
        QRectF braceBox(7.0, 2.0, 10.0, 20.0);
        const QPainterPath brace = ShapePathBuilder::BracePath(braceBox);
        if (preset == QStringLiteral("braceRight")) {
            QTransform mirror;
            mirror.translate(braceBox.left() + braceBox.right(), 0.0);
            mirror.scale(-1.0, 1.0);
            painter.drawPath(mirror.map(brace));
        } else {
            painter.drawPath(brace);
        }
    } else if (preset == QStringLiteral("lineArrow")) {
        // 开口箭头图标：箭杆只画到箭头根部，两条 45° 翼线在箭尖汇合，
        // 与插入时的 OPEN_ARROW 一致，也不会被箭杆穿过而糊成一团。
        const qreal y = box.center().y();
        const qreal tip = box.right() - 1.0;
        const qreal base = tip - 5.5;
        painter.drawLine(QPointF(box.left(), y), QPointF(base, y));
        painter.drawLine(QPointF(base, y - 5.5), QPointF(tip, y));
        painter.drawLine(QPointF(base, y + 5.5), QPointF(tip, y));
    }
    painter.end();

    return QIcon(pixmap);
}
