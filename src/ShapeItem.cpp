#include "I18n.h"

#include "ShapeItem.h"

#include <QJsonObject>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QTransform>
#include <QtMath>

namespace {

QRectF normalizedRect(const QRectF& rect)
{
    return rect.normalized();
}

QPainterPath bracePath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0)
        return QPainterPath();

    const qreal xOuter = r.left() + w * 0.82;
    const qreal xMid = r.left() + w * 0.60;
    const qreal xTip = r.left() + w * 0.16;

    QPainterPath path;
    path.moveTo(xOuter, r.top());
    path.cubicTo(xMid, r.top() + h * 0.025,
                 xMid, r.top() + h * 0.14,
                 xMid, r.top() + h * 0.30);
    path.cubicTo(xMid, r.top() + h * 0.43,
                 xTip, r.top() + h * 0.42,
                 xTip, r.center().y());
    path.cubicTo(xTip, r.top() + h * 0.58,
                 xMid, r.top() + h * 0.57,
                 xMid, r.top() + h * 0.70);
    path.cubicTo(xMid, r.top() + h * 0.86,
                 xMid, r.bottom() - h * 0.025,
                 xOuter, r.bottom());
    return path;
}

QPainterPath starPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const QPointF center = r.center();
    const qreal rx = r.width() / 2.0;
    const qreal ry = r.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0)
        return QPainterPath();

    QPainterPath path;
    constexpr int points = 10;
    for (int i = 0; i < points; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
        const qreal factor = (i % 2 == 0) ? 1.0 : 0.42;
        const QPointF point(center.x() + qCos(angle) * rx * factor,
                            center.y() + qSin(angle) * ry * factor);
        if (i == 0)
            path.moveTo(point);
        else
            path.lineTo(point);
    }
    path.closeSubpath();
    return path;
}

QPainterPath calloutPath(const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    const qreal w = r.width();
    const qreal h = r.height();
    if (w <= 0.0 || h <= 0.0)
        return QPainterPath();

    const QRectF body(r.left(), r.top(), w, h * 0.78);
    const qreal radius = qMin(w, h) * 0.16;
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(body, radius, radius);

    QPainterPath tail;
    tail.moveTo(r.left() + w * 0.20, body.bottom() - 0.5);
    tail.lineTo(r.left() + w * 0.12, r.bottom());
    tail.lineTo(r.left() + w * 0.38, body.bottom() - 0.5);
    tail.closeSubpath();

    return bodyPath.united(tail);
}

constexpr qreal kPatternPenWidth = 1.6;

qreal patternSpacing(const QRectF& rect)
{
    return qBound<qreal>(5.0, qMax(rect.width(), rect.height()) * 0.03, 12.0);
}

bool isPatternFill(FillStyle style)
{
    return style == FillStyle::Hachure || style == FillStyle::ReverseHachure
        || style == FillStyle::CrossHatch || style == FillStyle::GridHatch
        || style == FillStyle::Honeycomb;
}

// 填充纹理的中心线。绘制与遮挡计算共用这一份几何，保证看到的和“挡住别人的”一致。
QPainterPath patternCenterLines(FillStyle style, const QRectF& rect)
{
    QPainterPath path;
    if (rect.isEmpty())
        return path;

    const qreal spacing = patternSpacing(rect);
    const qreal h = rect.height();

    if (style == FillStyle::Honeycomb) {
        // 尖顶六边形蜂窝：行距 1.5r、列距 √3r，相邻行错开半个单元。
        const qreal radius = qMax<qreal>(4.0, spacing * 1.15);
        const qreal hexWidth = qSqrt(3.0) * radius;
        const qreal rowStep = 1.5 * radius;
        int row = 0;
        for (qreal cy = rect.top() - radius; cy < rect.bottom() + radius;
             cy += rowStep, ++row) {
            const qreal offset = (row % 2) ? hexWidth * 0.5 : 0.0;
            for (qreal cx = rect.left() - hexWidth + offset; cx < rect.right() + hexWidth;
                 cx += hexWidth) {
                for (int corner = 0; corner < 6; ++corner) {
                    const qreal angle = M_PI / 2.0 + corner * M_PI / 3.0;
                    const QPointF point(cx + radius * qCos(angle), cy + radius * qSin(angle));
                    if (corner == 0)
                        path.moveTo(point);
                    else
                        path.lineTo(point);
                }
                path.closeSubpath();
            }
        }
        return path;
    }

    if (style == FillStyle::GridHatch) {
        for (qreal y = rect.top() - spacing; y < rect.bottom() + spacing; y += spacing) {
            path.moveTo(rect.left() - spacing, y);
            path.lineTo(rect.right() + spacing, y);
        }
        for (qreal x = rect.left() - spacing; x < rect.right() + spacing; x += spacing) {
            path.moveTo(x, rect.top() - spacing);
            path.lineTo(x, rect.bottom() + spacing);
        }
        return path;
    }

    // 斜线 / 反斜线 / 交叉线。两组线都从左边外一个“高度”开始，才能铺满左下与右上角。
    const bool forward = style == FillStyle::Hachure || style == FillStyle::CrossHatch;
    const bool reverse = style == FillStyle::ReverseHachure || style == FillStyle::CrossHatch;
    if (forward) {
        for (qreal x = rect.left() - h; x < rect.right() + spacing; x += spacing) {
            path.moveTo(x, rect.bottom());
            path.lineTo(x + h, rect.top());
        }
    }
    if (reverse) {
        for (qreal x = rect.left() - h; x < rect.right() + spacing; x += spacing) {
            path.moveTo(x, rect.top());
            path.lineTo(x + h, rect.bottom());
        }
    }
    return path;
}

void paintPatternFill(QPainter* painter, const QPainterPath& clip, const QRectF& rect,
                      const QColor& color, FillStyle style)
{
    QColor lineColor = color;
    if (lineColor.alpha() < 120)
        lineColor.setAlpha(120);

    painter->save();
    // 必须用 IntersectClip：ReplaceClip 会把 drawItems() 里为“被遮挡裁剪”设置的区域
    // 整个替换掉，导致纹理线条画到本该被上层纯色图元遮住的地方。
    painter->setClipPath(clip, Qt::IntersectClip);
    QPen pen(lineColor, kPatternPenWidth, Qt::SolidLine, Qt::FlatCap);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(patternCenterLines(style, rect));
    painter->restore();
}

} // namespace

QList<ShapeType> allShapeTypes()
{
    return {
        ShapeType::Rectangle,
        ShapeType::RoundedRectangle,
        ShapeType::Ellipse,
        ShapeType::Triangle,
        ShapeType::Diamond,
        ShapeType::Star,
        ShapeType::Arrow,
        ShapeType::DoubleArrow,
        ShapeType::Line,
        ShapeType::BraceLeft,
        ShapeType::BraceRight,
        ShapeType::Callout
    };
}

QString shapeTypeName(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return uiText(QStringLiteral("矩形"));
    case ShapeType::RoundedRectangle:
        return uiText(QStringLiteral("圆角矩形"));
    case ShapeType::Ellipse:
        return uiText(QStringLiteral("椭圆"));
    case ShapeType::Triangle:
        return uiText(QStringLiteral("三角形"));
    case ShapeType::Diamond:
        return uiText(QStringLiteral("菱形"));
    case ShapeType::Star:
        return uiText(QStringLiteral("五角星"));
    case ShapeType::Arrow:
        return uiText(QStringLiteral("箭头"));
    case ShapeType::DoubleArrow:
        return uiText(QStringLiteral("双向箭头"));
    case ShapeType::Line:
        return uiText(QStringLiteral("直线"));
    case ShapeType::BraceLeft:
        return uiText(QStringLiteral("左大括号"));
    case ShapeType::BraceRight:
        return uiText(QStringLiteral("右大括号"));
    case ShapeType::Callout:
        return uiText(QStringLiteral("对话气泡"));
    }
    return uiText(QStringLiteral("矩形"));
}

QString shapeTypeToString(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
        return QStringLiteral("rectangle");
    case ShapeType::RoundedRectangle:
        return QStringLiteral("rounded_rectangle");
    case ShapeType::Ellipse:
        return QStringLiteral("ellipse");
    case ShapeType::Triangle:
        return QStringLiteral("triangle");
    case ShapeType::Diamond:
        return QStringLiteral("diamond");
    case ShapeType::Star:
        return QStringLiteral("star");
    case ShapeType::Arrow:
        return QStringLiteral("arrow");
    case ShapeType::DoubleArrow:
        return QStringLiteral("double_arrow");
    case ShapeType::Line:
        return QStringLiteral("line");
    case ShapeType::BraceLeft:
        return QStringLiteral("brace_left");
    case ShapeType::BraceRight:
        return QStringLiteral("brace_right");
    case ShapeType::Callout:
        return QStringLiteral("callout");
    }
    return QStringLiteral("rectangle");
}

ShapeType shapeTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("rounded_rectangle")
        || normalized == QStringLiteral("roundedrect")
        || normalized == QStringLiteral("roundrect")) {
        return ShapeType::RoundedRectangle;
    }
    if (normalized == QStringLiteral("ellipse") || normalized == QStringLiteral("circle")
        || normalized == QStringLiteral("oval")) {
        return ShapeType::Ellipse;
    }
    if (normalized == QStringLiteral("triangle"))
        return ShapeType::Triangle;
    if (normalized == QStringLiteral("diamond") || normalized == QStringLiteral("rhombus"))
        return ShapeType::Diamond;
    if (normalized == QStringLiteral("star"))
        return ShapeType::Star;
    if (normalized == QStringLiteral("arrow"))
        return ShapeType::Arrow;
    if (normalized == QStringLiteral("double_arrow") || normalized == QStringLiteral("doublearrow")
        || normalized == QStringLiteral("bidirectional")) {
        return ShapeType::DoubleArrow;
    }
    if (normalized == QStringLiteral("line"))
        return ShapeType::Line;
    if (normalized == QStringLiteral("brace_left") || normalized == QStringLiteral("leftbrace")
        || normalized == QStringLiteral("braceleft")) {
        return ShapeType::BraceLeft;
    }
    if (normalized == QStringLiteral("brace_right") || normalized == QStringLiteral("rightbrace")
        || normalized == QStringLiteral("braceright")) {
        return ShapeType::BraceRight;
    }
    if (normalized == QStringLiteral("callout") || normalized == QStringLiteral("bubble")
        || normalized == QStringLiteral("speech")) {
        return ShapeType::Callout;
    }
    if (normalized == QStringLiteral("rect") || normalized == QStringLiteral("rectangle"))
        return ShapeType::Rectangle;
    return ShapeType::Rectangle;
}

QSizeF defaultShapeSize(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
        return QSizeF(300.0, 190.0);
    case ShapeType::Ellipse:
        // 插入即为标准圆形；编辑模式下拖动控制点可拉伸成椭圆。
        return QSizeF(200.0, 200.0);
    case ShapeType::Triangle:
        return QSizeF(260.0, 210.0);
    case ShapeType::Diamond:
        return QSizeF(230.0, 180.0);
    case ShapeType::Star:
        return QSizeF(190.0, 190.0);
    case ShapeType::Arrow:
        return QSizeF(340.0, 120.0);
    case ShapeType::DoubleArrow:
        return QSizeF(380.0, 120.0);
    case ShapeType::Line:
        return QSizeF(320.0, 72.0);
    case ShapeType::BraceLeft:
    case ShapeType::BraceRight:
        return QSizeF(100.0, 300.0);
    case ShapeType::Callout:
        return QSizeF(300.0, 200.0);
    }
    return QSizeF(300.0, 190.0);
}

bool shapeTypeIsFilled(ShapeType type)
{
    switch (type) {
    case ShapeType::Rectangle:
    case ShapeType::RoundedRectangle:
    case ShapeType::Ellipse:
    case ShapeType::Triangle:
    case ShapeType::Diamond:
    case ShapeType::Star:
    case ShapeType::Callout:
        return true;
    case ShapeType::Arrow:
    case ShapeType::DoubleArrow:
    case ShapeType::Line:
    case ShapeType::BraceLeft:
    case ShapeType::BraceRight:
        return false;
    }
    return false;
}

bool shapeTypeIsStrokeOnly(ShapeType type)
{
    return type == ShapeType::Line || type == ShapeType::BraceLeft
        || type == ShapeType::BraceRight;
}

QList<FillStyle> allFillStyles()
{
    return { FillStyle::None,       FillStyle::Solid,    FillStyle::Hachure,
             FillStyle::ReverseHachure, FillStyle::CrossHatch, FillStyle::GridHatch,
             FillStyle::Honeycomb };
}

QString fillStyleName(FillStyle style)
{
    switch (style) {
    case FillStyle::None:
        return uiText(QStringLiteral("无填充"));
    case FillStyle::Solid:
        return uiText(QStringLiteral("纯色"));
    case FillStyle::Hachure:
        return uiText(QStringLiteral("斜线"));
    case FillStyle::ReverseHachure:
        return uiText(QStringLiteral("反斜线"));
    case FillStyle::CrossHatch:
        return uiText(QStringLiteral("交叉线"));
    case FillStyle::GridHatch:
        return uiText(QStringLiteral("正交叉线"));
    case FillStyle::Honeycomb:
        return uiText(QStringLiteral("六边形"));
    }
    return uiText(QStringLiteral("纯色"));
}

QString fillStyleToString(FillStyle style)
{
    switch (style) {
    case FillStyle::None:
        return QStringLiteral("none");
    case FillStyle::Solid:
        return QStringLiteral("solid");
    case FillStyle::Hachure:
        return QStringLiteral("hachure");
    case FillStyle::ReverseHachure:
        return QStringLiteral("reverse_hatch");
    case FillStyle::CrossHatch:
        return QStringLiteral("cross_hatch");
    case FillStyle::GridHatch:
        return QStringLiteral("grid_hatch");
    case FillStyle::Honeycomb:
        return QStringLiteral("honeycomb");
    }
    return QStringLiteral("solid");
}

FillStyle fillStyleFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("none") || normalized == QStringLiteral("transparent"))
        return FillStyle::None;
    if (normalized == QStringLiteral("hachure") || normalized == QStringLiteral("hatch")
        || normalized == QStringLiteral("diagonal")) {
        return FillStyle::Hachure;
    }
    if (normalized == QStringLiteral("reverse_hatch")
        || normalized == QStringLiteral("reverse_hachure")
        || normalized == QStringLiteral("reverse")
        || normalized == QStringLiteral("backslash")) {
        return FillStyle::ReverseHachure;
    }
    if (normalized == QStringLiteral("cross_hatch") || normalized == QStringLiteral("crosshatch")
        || normalized == QStringLiteral("cross") || normalized == QStringLiteral("zigzag")) {
        return FillStyle::CrossHatch;
    }
    if (normalized == QStringLiteral("grid_hatch") || normalized == QStringLiteral("grid")
        || normalized == QStringLiteral("cross_grid") || normalized == QStringLiteral("orthogonal")) {
        return FillStyle::GridHatch;
    }
    if (normalized == QStringLiteral("honeycomb") || normalized == QStringLiteral("hexagon")
        || normalized == QStringLiteral("hex")) {
        return FillStyle::Honeycomb;
    }
    if (normalized == QStringLiteral("solid") || normalized == QStringLiteral("color"))
        return FillStyle::Solid;
    return FillStyle::Solid;
}

QIcon makeShapeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale),
                   static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    QRectF rect(3.0, 4.0, 18.0, 16.0);
    if (type == ShapeType::Ellipse)
        rect = QRectF(4.0, 4.0, 16.0, 16.0);
    else if (type == ShapeType::BraceLeft || type == ShapeType::BraceRight)
        rect = QRectF(6.0, 2.0, 12.0, 20.0);
    else if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow
             || type == ShapeType::Line) {
        rect = QRectF(2.0, 7.0, 20.0, 10.0);
    }

    const QPainterPath path = ShapeItem::pathFor(type, rect);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    } else if (shapeTypeIsStrokeOnly(type)) {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
    } else {
        painter.setPen(pen);
        QColor fill = color;
        fill.setAlpha(50);
        painter.setBrush(fill);
    }
    painter.drawPath(path);
    painter.end();

    return QIcon(pixmap);
}

ShapeItem::ShapeItem(ShapeType type, QGraphicsItem* parent)
    : BoardItem(parent)
    , m_type(type)
{
    m_fillColor = m_strokeColor;
    m_fillColor.setAlpha(36);
    setContentSize(defaultShapeSize(type));
}

QString ShapeItem::displayName() const
{
    return shapeTypeName(m_type);
}

void ShapeItem::setShapeType(ShapeType type)
{
    if (m_type == type)
        return;
    m_type = type;
    update();
}

void ShapeItem::setStyle(const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth)
{
    m_strokeColor = strokeColor.isValid() ? strokeColor : QColor(30, 41, 59);
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_strokeWidth = qBound<qreal>(0.5, strokeWidth, 48.0);
    update();
}

void ShapeItem::setFill(const QColor& fillColor, FillStyle style)
{
    m_fillColor = fillColor.isValid() ? fillColor : QColor(Qt::transparent);
    m_fillStyle = style;
    update();
}

void ShapeItem::setFillStyle(FillStyle style)
{
    if (m_fillStyle == style)
        return;
    m_fillStyle = style;
    update();
}

ShapeItemState ShapeItem::state() const
{
    ShapeItemState result;
    result.type = m_type;
    result.strokeColor = m_strokeColor;
    result.fillColor = m_fillColor;
    result.fillStyle = m_fillStyle;
    result.strokeWidth = m_strokeWidth;
    result.pos = pos();
    result.size = contentSize();
    return result;
}

void ShapeItem::applyState(const ShapeItemState& state)
{
    m_type = state.type;
    m_fillStyle = state.fillStyle;
    setStyle(state.strokeColor, state.fillColor, state.strokeWidth);
    setPos(state.pos);
    setContentSize(state.size.isEmpty() ? defaultShapeSize(state.type) : state.size);
    update();
}

QPainterPath ShapeItem::shape() const
{
    const QPainterPath path = pathFor(m_type, contentRect());
    if (path.isEmpty())
        return selectionHandleShape();

    QPainterPath result;
    if (shapeTypeIsStrokeOnly(m_type)) {
        QPainterPathStroker stroker;
        stroker.setWidth(qMax<qreal>(5.0, m_strokeWidth + 6.0));
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        result = stroker.createStroke(path);
    } else if (m_type == ShapeType::Arrow || m_type == ShapeType::DoubleArrow) {
        // Painted with a filled pen-free path, so the outline is exact.
        result = path;
    } else {
        // Filled shapes are stroked with m_strokeWidth, which paints up to
        // strokeWidth/2 outside the path. Without this ring the visible border
        // of a thick outline is not part of the hit area.
        QPainterPathStroker stroker;
        stroker.setWidth(qMax<qreal>(2.0, m_strokeWidth + 2.0));
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        result = stroker.createStroke(path);
        result.addPath(path);
    }

    result.addPath(selectionHandleShape());
    return result;
}

QPainterPath ShapeItem::outlineShape() const
{
    const QPainterPath path = pathFor(m_type, contentRect());
    return path.isEmpty() ? BoardItem::outlineShape() : path;
}

QPainterPath ShapeItem::paintedShape() const
{
    const QPainterPath path = pathFor(m_type, contentRect());
    if (path.isEmpty())
        return QPainterPath();

    // 描边本身也会遮挡下方图元。
    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(1.0, m_strokeWidth));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath result = stroker.createStroke(path);

    // 有面积的图形只有“纯色填充”时才会完整遮住下层：纯色（含半透明纯色）的轮廓内部
    // 整个算作遮挡区，下层颜色完全不画，避免叠出混合色。
    // 无填充、斜线/交叉线/六边形等纹理填充不遮挡下层的轮廓，下层照常可见（查看模式也一样）；
    // 描边本身仍然遮挡笔画扫过的区域，箭头按实心轮廓整体遮挡。
    if (!shapeTypeIsStrokeOnly(m_type)) {
        const bool solidFill = m_fillStyle == FillStyle::Solid && m_fillColor.alpha() > 0;
        const bool solidShape = m_type == ShapeType::Arrow || m_type == ShapeType::DoubleArrow;
        if (solidFill || solidShape)
            result = result.united(path);
    }
    return result;
}

QPainterPath ShapeItem::pathFor(ShapeType type, const QRectF& rect)
{
    const QRectF r = normalizedRect(rect);
    if (r.width() <= 0.0 || r.height() <= 0.0)
        return QPainterPath();

    QPainterPath path;
    switch (type) {
    case ShapeType::Rectangle:
        path.addRect(r);
        break;
    case ShapeType::RoundedRectangle: {
        const qreal radius = qMin(r.width(), r.height()) * 0.16;
        path.addRoundedRect(r, radius, radius);
        break;
    }
    case ShapeType::Ellipse:
        path.addEllipse(r);
        break;
    case ShapeType::Triangle:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
        break;
    case ShapeType::Diamond:
        path.moveTo(r.center().x(), r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.center().x(), r.bottom());
        path.lineTo(r.left(), r.center().y());
        path.closeSubpath();
        break;
    case ShapeType::Star:
        path = starPath(r);
        break;
    case ShapeType::Arrow: {
        const qreal headLength = qMin(r.width() * 0.45,
                                      qMax(r.height() * 0.75, r.width() * 0.22));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal headBase = r.right() - headLength;
        path.moveTo(r.left(), cy - shaftHalf);
        path.lineTo(headBase, cy - shaftHalf);
        path.lineTo(headBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(headBase, r.bottom());
        path.lineTo(headBase, cy + shaftHalf);
        path.lineTo(r.left(), cy + shaftHalf);
        path.closeSubpath();
        break;
    }
    case ShapeType::DoubleArrow: {
        const qreal headLength = qMin(r.width() * 0.45,
                                      qMin(r.width() * 0.30,
                                           qMax(r.height() * 0.70, r.width() * 0.16)));
        const qreal shaftHalf = qMin(r.height() * 0.17, headLength * 0.45);
        const qreal cy = r.center().y();
        const qreal leftBase = r.left() + headLength;
        const qreal rightBase = r.right() - headLength;
        path.moveTo(r.left(), cy);
        path.lineTo(leftBase, r.top());
        path.lineTo(leftBase, cy - shaftHalf);
        path.lineTo(rightBase, cy - shaftHalf);
        path.lineTo(rightBase, r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(rightBase, r.bottom());
        path.lineTo(rightBase, cy + shaftHalf);
        path.lineTo(leftBase, cy + shaftHalf);
        path.lineTo(leftBase, r.bottom());
        path.closeSubpath();
        break;
    }
    case ShapeType::Line:
        path.moveTo(r.left(), r.center().y());
        path.lineTo(r.right(), r.center().y());
        break;
    case ShapeType::BraceLeft:
        path = bracePath(r);
        break;
    case ShapeType::BraceRight: {
        const QPainterPath left = bracePath(r);
        QTransform mirror;
        mirror.translate(r.left() + r.right(), 0.0);
        mirror.scale(-1.0, 1.0);
        path = mirror.map(left);
        break;
    }
    case ShapeType::Callout:
        path = calloutPath(r);
        break;
    }
    return path;
}

void ShapeItem::paintContent(QPainter* painter)
{
    const QRectF rect = contentRect();
    if (rect.isEmpty())
        return;

    const QPainterPath path = pathFor(m_type, rect);
    if (path.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_type == ShapeType::Arrow || m_type == ShapeType::DoubleArrow) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_strokeColor);
        painter->drawPath(path);
    } else if (shapeTypeIsStrokeOnly(m_type)) {
        QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine,
                 Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    } else {
        if (isPatternFill(m_fillStyle))
            paintPatternFill(painter, path, rect, m_fillColor, m_fillStyle);

        QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine,
                 Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        if (m_fillStyle == FillStyle::Solid && m_fillColor.alpha() > 0)
            painter->setBrush(m_fillColor);
        else
            painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }
    painter->restore();
}

QJsonObject ShapeItem::toJson() const
{
    QJsonObject json = baseJson();
    json.insert(QStringLiteral("shapeType"), shapeTypeToString(m_type));
    json.insert(QStringLiteral("strokeColor"), m_strokeColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillColor"), m_fillColor.name(QColor::HexArgb));
    json.insert(QStringLiteral("fillStyle"), fillStyleToString(m_fillStyle));
    json.insert(QStringLiteral("strokeWidth"), m_strokeWidth);
    return json;
}

void ShapeItem::fromJson(const QJsonObject& json)
{
    m_type = shapeTypeFromString(json.value(QStringLiteral("shapeType")).toString());

    m_strokeColor = QColor(json.value(QStringLiteral("strokeColor"))
                               .toString(QStringLiteral("#ff1e293b")));
    if (!m_strokeColor.isValid())
        m_strokeColor = QColor(30, 41, 59);

    if (json.contains(QStringLiteral("fillColor"))) {
        m_fillColor = QColor(json.value(QStringLiteral("fillColor")).toString());
        if (!m_fillColor.isValid())
            m_fillColor = QColor(Qt::transparent);
    } else {
        m_fillColor = m_strokeColor;
        m_fillColor.setAlpha(36);
    }

    if (json.contains(QStringLiteral("fillStyle"))) {
        m_fillStyle = fillStyleFromString(json.value(QStringLiteral("fillStyle")).toString());
    } else {
        m_fillStyle = m_fillColor.alpha() > 0 ? FillStyle::Solid : FillStyle::None;
    }

    m_strokeWidth = qBound<qreal>(
        0.5, json.value(QStringLiteral("strokeWidth")).toDouble(4.0), 48.0);

    applyBaseJson(json);
    if (contentSize().isEmpty())
        setContentSize(defaultShapeSize(m_type));
    update();
}
