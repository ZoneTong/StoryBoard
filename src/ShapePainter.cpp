#include "ShapePainter.h"

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
           style == FillStyle::GridHatch || style == FillStyle::Honeycomb;
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
    painter->setBrush(Qt::NoBrush);
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

    if (shapeTypeIsStrokeOnly(type)) {
        QPen pen(strokeColor, strokeWidth, strokeStyle, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    } else {
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
    }
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
    } else if (type == ShapeType::BraceLeft || type == ShapeType::BraceRight) {
        rect = QRectF(6.0, 2.0, 12.0, 20.0);
    } else if (type == ShapeType::Arrow || type == ShapeType::DoubleArrow || type == ShapeType::Line) {
        rect = QRectF(2.0, 7.0, 20.0, 10.0);
    } else if (type == ShapeType::Sector || type == ShapeType::Arc) {
        // 扇形 / 弧线的图标必须与插入后的默认形态同一套角度（30° 起、120° 扫过），
        // 否则“点图标得到另一个形状”。只把椭圆放大到整格，避免在这排里显得过小。
        rect = QRectF(1.0, 1.0, 22.0, 22.0);
        iconStartAngle = 30.0;
        iconSweepAngle = 120.0;
    }

    const QPainterPath path = ShapeItem::pathFor(type, rect, -1.0, iconStartAngle, iconSweepAngle, -1.0);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    // 常见图形图标统一画成空心轮廓：只有描边，不填底色。
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.end();

    return QIcon(pixmap);
}
