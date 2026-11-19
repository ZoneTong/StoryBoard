#include "ShapePainter.h"

#include "EllipseShape.h"

#include <QPixmap>
#include <QtMath>

namespace {

constexpr qreal kPatternPenWidth = 1.6;

qreal patternSpacing(const QRectF& rect)
{
    return qBound<qreal>(5.0, qMax(rect.width(), rect.height()) * 0.03, 12.0);
}

} // namespace

QPainterPath ShapePainter::centerLines(FillStyle style, const QRectF& rect)
{
    QPainterPath path;
    if (rect.isEmpty()) {
        return path;
    }

    const qreal spacing = patternSpacing(rect);
    const qreal h = rect.height();

    if (style == FillStyle::Honeycomb) {
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

    if (style == FillStyle::HorizontalHatch) {
        for (qreal y = rect.top() - spacing; y < rect.bottom() + spacing; y += spacing) {
            path.moveTo(rect.left() - spacing, y);
            path.lineTo(rect.right() + spacing, y);
        }
        return path;
    }

    if (style == FillStyle::VerticalHatch) {
        for (qreal x = rect.left() - spacing; x < rect.right() + spacing; x += spacing) {
            path.moveTo(x, rect.top() - spacing);
            path.lineTo(x, rect.bottom() + spacing);
        }
        return path;
    }

    if (style == FillStyle::Brick) {
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

    if (style == FillStyle::Dots) {
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

bool ShapePainter::isPatternFill(FillStyle style)
{
    return style == FillStyle::Hachure || style == FillStyle::ReverseHachure || style == FillStyle::CrossHatch ||
           style == FillStyle::GridHatch || style == FillStyle::Honeycomb || style == FillStyle::HorizontalHatch ||
           style == FillStyle::VerticalHatch || style == FillStyle::Brick || style == FillStyle::Dots;
}

void ShapePainter::paintPatternFill(QPainter* painter, const QPainterPath& clip, const QRectF& rect,
                                    const QColor& color, FillStyle style)
{
    QColor lineColor = color;
    if (lineColor.alpha() < 120) {
        lineColor.setAlpha(120);
    }

    painter->save();
    // 必须用 IntersectClip：ReplaceClip 会把 drawItems() 里为“被遮挡裁剪”设置的区域
    // 整个替换掉，导致纹理线条画到本该被上层纯色图元遮住的地方。
    painter->setClipPath(clip, Qt::IntersectClip);
    QPen pen(lineColor, kPatternPenWidth, Qt::SolidLine, Qt::FlatCap);
    painter->setPen(pen);
    // 点阵需要实心圆点，其余图案只描线。
    painter->setBrush(style == FillStyle::Dots ? QBrush(lineColor) : QBrush(Qt::NoBrush));
    painter->drawPath(centerLines(style, rect));
    painter->restore();
}

void ShapePainter::paintShape(QPainter* painter, const QPainterPath& path, const QRectF& rect, ShapeType type,
                              const QColor& strokeColor, const QColor& fillColor, qreal strokeWidth,
                              Qt::PenStyle strokeStyle, FillStyle fillStyle, qreal opacity)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setOpacity(painter->opacity() * opacity);

    if (isPatternFill(fillStyle)) {
        paintPatternFill(painter, path, rect, fillColor, fillStyle);
    }

    QPen pen(strokeColor, strokeWidth, strokeStyle, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    if (fillStyle == FillStyle::Solid && fillColor.alpha() > 0) {
        painter->setBrush(fillColor);
    } else {
        painter->setBrush(Qt::NoBrush);
    }
    painter->drawPath(path);
    painter->restore();
}

QIcon ShapePainter::makeIcon(ShapeType type, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    QRectF rect(3.0, 4.0, 18.0, 16.0);
    qreal iconStartAngle = 30.0;
    qreal iconSweepAngle = 120.0;
    if (type == ShapeType::Ellipse) {
        rect = QRectF(4.0, 4.0, 16.0, 16.0);
    } else if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow) {
        rect = QRectF(2.0, 7.0, 20.0, 10.0);
    } else if (type == ShapeType::Sector) {
        // 扇形 / 弧线的图标必须与插入后的默认形态同一套角度（30° 起、120° 扫过），
        // 否则“点图标得到另一个形状”。只把椭圆放大到整格，避免在这排里显得过小。
        rect = QRectF(1.0, 1.0, 22.0, 22.0);
        iconStartAngle = 30.0;
        iconSweepAngle = 120.0;
    }

    QPainterPath path = ShapeItem::pathFor(type, rect, -1.0, iconStartAngle, iconSweepAngle);
    // 需求 7：扇形 / 弧线的图标按包围盒重新居中，视觉重心落在 (12,12)，
    // 并等比缩到不超过 18×18；笔宽不跟缩放走，保持与同排图标一致。
    if (type == ShapeType::Sector) {
        const QRectF bounds = path.boundingRect();
        if (!bounds.isEmpty()) {
            qreal scale =
                qMin<qreal>(18.0 / qMax<qreal>(bounds.width(), 1.0), 18.0 / qMax<qreal>(bounds.height(), 1.0));
            scale = qBound<qreal>(0.5, scale, 1.4);
            QTransform transform;
            transform.translate(12.0, 12.0);
            transform.scale(scale, scale);
            transform.translate(-bounds.center().x(), -bounds.center().y());
            path = transform.map(path);
        }
    }
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    // 常见图形图标统一画成空心轮廓：只有描边，不填底色。
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.end();

    return QIcon(pixmap);
}

QIcon ShapePainter::makeCurveIcon(CurveKind kind, const QColor& color, int logicalSize)
{
    constexpr qreal scale = 2.0;
    QPixmap pixmap(static_cast<int>(logicalSize * scale), static_cast<int>(logicalSize * scale));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(scale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);

    // 24px 栅格里的一笔开放描线：与插入后的默认形态同一套角度 / 周期。
    QPainterPath path;
    if (kind == CurveKind::Arc) {
        path = EllipseShape(QRectF(1.5, 1.5, 21.0, 21.0)).arc(30.0, 120.0);
    } else if (kind == CurveKind::Sine) {
        constexpr int steps = 64;
        for (int i = 0; i <= steps; ++i) {
            const qreal t = qreal(i) / qreal(steps);
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
    painter.setPen(QPen(color, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    return QIcon(pixmap);
}
