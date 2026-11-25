#include "items/geometry/ShapePainter.h"

#include "items/geometry/EllipseGeometry.h"
#include "items/properties/FillStyleMeta.h"

#include <QPixmap>
#include <QtMath>

namespace {

constexpr qreal PATTERN_PEN_WIDTH = 1.6;

// 图标墨迹归心：按包围盒（含描边外扩）把图形平移到 24px 栅格中心，并等比限幅，
// 扇形与弧线共用一份，避免两条路各写一遍、下次只修一处。
QPainterPath CenterIconPath(const QPainterPath& path, qreal penWidth, qreal bound = 18.0, qreal maxScale = 1.4)
{
    const qreal margin = penWidth / 2.0;
    const QRectF bounds = path.boundingRect().adjusted(-margin, -margin, margin, margin);
    if (bounds.isEmpty()) {
        return path;
    }
    qreal scale = qMin<qreal>(bound / qMax<qreal>(bounds.width(), 1.0), bound / qMax<qreal>(bounds.height(), 1.0));
    scale = qBound<qreal>(0.5, scale, maxScale);
    QTransform transform;
    transform.translate(12.0, 12.0);
    transform.scale(scale, scale);
    transform.translate(-bounds.center().x(), -bounds.center().y());
    return transform.map(path);
}

qreal PatternSpacing(const QRectF& rect)
{
    return qBound<qreal>(5.0, qMax(rect.width(), rect.height()) * 0.03, 12.0);
}

} // namespace

QPainterPath ShapePainter::CenterLines(FillStyle style, const QRectF& rect)
{
    QPainterPath path;
    if (rect.isEmpty()) {
        return path;
    }

    const qreal spacing = PatternSpacing(rect);
    const qreal h = rect.height();

    if (style == FillStyle::HONEYCOMB) {
        // 尖顶六边形蜂窝：行距 1.5r、列距 √3r，相邻行错开半个单元。
        const qreal radius = qMax<qreal>(4.0, spacing * 1.15);
        const qreal hexWidth = qSqrt(3.0) * radius;
        const qreal rowStep = 1.5 * radius;
        int row = 0;
        for (qreal cy = rect.top() - radius; cy < rect.bottom() + radius; cy += rowStep, ++row) {
            const qreal offset = (row % 2) ? hexWidth * 0.5 : 0.0;
            for (qreal cx = rect.left() - hexWidth + offset; cx < rect.right() + hexWidth; cx += hexWidth) {
                for (int corner = 0; corner < 6; ++corner) {
                    const qreal angle = M_PI / 2.0 + corner * M_PI / 3.0;
                    const QPointF point(cx + radius * qCos(angle), cy + radius * qSin(angle));
                    if (corner == 0) {
                        path.moveTo(point);
                    } else {
                        path.lineTo(point);
                    }
                }
                path.closeSubpath();
            }
        }
        return path;
    }

    if (style == FillStyle::GRID_HATCH) {
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

    if (style == FillStyle::BRICK) {
        // 砖纹：逐行横线，竖缝只在行与行之间出现，奇偶行错开半块砖。
        const qreal brickWidth = spacing * 2.0;
        int row = 0;
        for (qreal y = rect.top() - spacing; y < rect.bottom() + spacing; y += spacing, ++row) {
            path.moveTo(rect.left() - spacing, y);
            path.lineTo(rect.right() + spacing, y);
            const qreal offset = (row % 2) ? brickWidth * 0.5 : 0.0;
            for (qreal x = rect.left() - spacing + offset; x < rect.right() + spacing; x += brickWidth) {
                path.moveTo(x, y);
                path.lineTo(x, y + spacing);
            }
        }
        return path;
    }

    if (style == FillStyle::DOTS) {
        // 点阵（50%）：圆点半径取间距的 1/4，圆心按棋盘交错排布。
        const qreal radius = qMax<qreal>(1.2, spacing * 0.25);
        int row = 0;
        for (qreal cy = rect.top() - spacing * 0.5; cy < rect.bottom() + spacing; cy += spacing, ++row) {
            const qreal offset = (row % 2) ? spacing * 0.5 : 0.0;
            for (qreal cx = rect.left() - spacing + offset; cx < rect.right() + spacing; cx += spacing) {
                path.addEllipse(QPointF(cx, cy), radius, radius);
            }
        }
        return path;
    }

    // 斜线 / 反斜线 / 交叉线。两组线都从左边外一个“高度”开始，才能铺满左下与右上角。
    const bool forward = style == FillStyle::HACHURE || style == FillStyle::CROSS_HATCH;
    const bool reverse = style == FillStyle::REVERSE_HACHURE || style == FillStyle::CROSS_HATCH;
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

bool ShapePainter::IsPatternFill(FillStyle style)
{
    return FillStyleMeta::IsPattern(style);
}

void ShapePainter::PaintPatternFill(QPainter* painter, const QPainterPath& clip, const QRectF& rect,
                                    const QColor& color, FillStyle style)
{
    QColor lineColor = color;
    if (lineColor.alpha() < 120) {
        lineColor.setAlpha(120);
    }

    painter->save();
    // 必须用 IntersectClip：ReplaceClip 会把 DrawItems() 里为“被遮挡裁剪”设置的区域
    // 整个替换掉，导致纹理线条画到本该被上层纯色图元遮住的地方。
    painter->setClipPath(clip, Qt::IntersectClip);
    QPen pen(lineColor, PATTERN_PEN_WIDTH, Qt::SolidLine, Qt::FlatCap);
    painter->setPen(pen);
    // 点阵需要实心圆点，其余图案只描线。
    painter->setBrush(style == FillStyle::DOTS ? QBrush(lineColor) : QBrush(Qt::NoBrush));
    painter->drawPath(CenterLines(style, rect));
    painter->restore();
}

void ShapePainter::PaintShape(QPainter* painter, const QPainterPath& path, const QRectF& rect, ShapeType type,
                              const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth,
                              Qt::PenStyle strokeStyle, FillStyle fillStyle)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (IsPatternFill(fillStyle)) {
        PaintPatternFill(painter, path, rect, fillColor, fillStyle);
    }

    QPen pen(strokeColor, strokeWidth, strokeStyle, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    if (fillStyle == FillStyle::SOLID && fillColor.alpha() > 0) {
        painter->setBrush(fillColor);
    } else {
        painter->setBrush(Qt::NoBrush);
    }
    painter->drawPath(path);
    painter->restore();
}

QIcon ShapePainter::MakeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    QRectF rect(3.0, 4.0, 18.0, 16.0);
    qreal iconStartAngle = 30.0;
    qreal iconSweepAngle = 120.0;
    if (type == ShapeType::ELLIPSE) {
        rect = QRectF(4.0, 4.0, 16.0, 16.0);
    } else if (type == ShapeType::ARROW || type == ShapeType::DOUBLE_ARROW) {
        rect = QRectF(2.0, 7.0, 20.0, 10.0);
    } else if (type == ShapeType::SECTOR) {
        // 扇形 / 弧线的图标必须与插入后的默认形态同一套角度（30° 起、120° 扫过），
        // 否则“点图标得到另一个形状”。只把椭圆放大到整格，避免在这排里显得过小。
        rect = QRectF(1.0, 1.0, 22.0, 22.0);
        iconStartAngle = 30.0;
        iconSweepAngle = 120.0;
    }

    QPainterPath path = ShapeAbstractItem::PathFor(type, rect, -1.0, iconStartAngle, iconSweepAngle);
    // 需求 7：扇形图标的墨迹按包围盒重新居中到 (12,12) 并等比限幅；弧线走同一份 helper。
    if (type == ShapeType::SECTOR) {
        path = CenterIconPath(path, 1.8);
    }
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    // 常见图形图标统一画成空心轮廓：只有描边，不填底色。
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.end();

    return QIcon(pixmap);
}

QIcon ShapePainter::MakeCurveIcon(LineGlyph glyph, const QColor& color, int logicalSize)
{
    constexpr qreal SCALE = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * SCALE), static_cast<int>(logicalSize * SCALE));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(SCALE);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    // 24px 栅格里的一笔开放描线：与插入后的默认形态同一套角度 / 周期。
    QPainterPath path;
    if (glyph == LineGlyph::ARC) {
        path = EllipseGeometry(QRectF(1.5, 1.5, 21.0, 21.0)).Arc(30.0, 120.0);
    } else if (glyph == LineGlyph::ANGLE) {
        // 角度线：顶点在中心，两条射线分别落在 30° / 150° 方向。
        const QRectF frame(1.5, 1.5, 21.0, 21.0);
        const QPointF center = frame.center();
        const qreal rx = frame.width() / 2.0;
        const qreal ry = frame.height() / 2.0;
        const auto tip = [&](qreal degrees) {
            const qreal radians = qDegreesToRadians(degrees);
            return QPointF(center.x() + rx * std::cos(radians), center.y() - ry * std::sin(radians));
        };
        path.moveTo(tip(30.0));
        path.lineTo(center);
        path.lineTo(tip(150.0));
    } else if (glyph == LineGlyph::SINE) {
        constexpr int STEPS = 64;
        for (int i = 0; i <= STEPS; ++i) {
            const qreal t = qreal(i) / qreal(STEPS);
            const QPointF point(2.5 + t * 19.0, 12.0 - qSin(t * 2.0 * M_PI) * 6.0);
            if (i == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
    } else {
        path.moveTo(2.5, 12.0);
        path.lineTo(21.5, 12.0);
    }
    // 三种形态统一归心：直线与正弦本就居中（平移量为 0，行为不变），弧线自动修正
    // 此前整块偏上约 7.9px 的问题；归心按包围盒计算，描边外扩也一并覆盖。
    path = CenterIconPath(path, 1.6);
    painter.setPen(QPen(color, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    return QIcon(pixmap);
}
